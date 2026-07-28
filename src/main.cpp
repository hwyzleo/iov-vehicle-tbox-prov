#include "prov_service.h"
#include "prov_log_adapter.h"
#include "framework_store.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <chrono>

// 信号处理器与主循环之间唯一的共享状态，必须是 lock-free 的
static std::atomic<bool> shutdown_requested(false);
// 记录触发停机的信号编号，供退出日志使用
static volatile sig_atomic_t received_signal = 0;

// 异步信号安全的信号处理器：
// 只允许 write(2) 与对 atomic/sig_atomic_t 的写入，
// 不得使用 iostream / printf / 分配内存 / 加锁
extern "C" void signal_handler(int signum) {
    received_signal = signum;
    shutdown_requested.store(true, std::memory_order_relaxed);

    static const char msg[] = "\n[signal] shutdown requested\n";
    // 忽略返回值：处理器内无法做有意义的错误处理
    ssize_t ignored = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)ignored;
}

// 使用 sigaction 而非 signal，避免不同平台上处置语义（是否自动重置、
// 是否重启系统调用）的差异
static bool install_signal_handlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    // 处理期间屏蔽其他停机信号，避免处理器重入
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGHUP);
    // SA_RESTART：让阻塞的系统调用自动重启，减少 EINTR 干扰
    sa.sa_flags = SA_RESTART;

    const int shutdown_signals[] = {SIGINT, SIGTERM, SIGHUP};
    for (int sig : shutdown_signals) {
        if (sigaction(sig, &sa, nullptr) != 0) {
            std::cerr << "FATAL: sigaction failed for signal " << sig << std::endl;
            return false;
        }
    }

    // 忽略 SIGPIPE：对端断开时由 write 返回 EPIPE 处理
    struct sigaction sa_ign;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    if (sigaction(SIGPIPE, &sa_ign, nullptr) != 0) {
        std::cerr << "FATAL: sigaction failed for SIGPIPE" << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    if (!install_signal_handlers()) {
        return 1;
    }
    
    // 加载框架配置
    auto err = CONFIG_MANAGER.load("prov");
    if (err != hwyz::config::ConfigError::kOk) {
        auto info = CONFIG_MANAGER.getLastError();
        std::cerr << "FATAL: Config load failed: " << info.message << std::endl;
        return 1;
    }
    
    // 读取日志配置
    auto cfg = CONFIG_SNAPSHOT;
    tbox::fw::log::LogConfig log_config;
    log_config.level = tbox::fw::log::LogLevel::kInfo;
    log_config.strict = true;  // fail-closed: Logger 初始化失败则退出
    
    // 从配置读取日志级别
    std::string log_level_str = cfg->getString("common.log.level", "info");
    if (log_level_str == "trace") log_config.level = tbox::fw::log::LogLevel::kTrace;
    else if (log_level_str == "debug") log_config.level = tbox::fw::log::LogLevel::kDebug;
    else if (log_level_str == "info") log_config.level = tbox::fw::log::LogLevel::kInfo;
    else if (log_level_str == "warn") log_config.level = tbox::fw::log::LogLevel::kWarn;
    else if (log_level_str == "error") log_config.level = tbox::fw::log::LogLevel::kError;
    
    // 初始化 Logger
    auto log_result = tbox::prov::ProvLogAdapter::init("prov", log_config);
    if (log_result.error != tbox::fw::log::LogError::kOk) {
        std::cerr << "FATAL: Logger init failed: " << log_result.error_message << std::endl;
        return 1;
    }
    
    // 记录初始化成功
    tbox::prov::ProvLogAdapter::service().info("prov.service.log_initialized",
        "Logger initialized successfully",
        {tbox::fw::log::Field("service", tbox::fw::log::FieldValue::makeString("prov")),
         tbox::fw::log::Field("sink_mode", tbox::fw::log::FieldValue::makeString("console"))}
    );
    
    tbox::prov::ProvLogAdapter::service().info("prov.service.starting",
        "TBOX PROV Service Starting"
    );
    
    // 从配置读取服务参数
    std::string store_root = cfg->getString("common.store.root", "/var/tbox");
    tbox::prov::ProvServiceConfig config;
    config.enable_write_protection = cfg->getBool("storage.enable_write_protection", true);
    config.max_retry_count = cfg->getInt("storage.max_retry_count", 3);
    config.ipc_socket_path = cfg->getString("prov.ipc.socket_path", "/tmp/tbox-prov.sock");

    // 读取 IPC 配置并注入 framework-ipc
    config.ipc_config.max_frame_bytes = static_cast<uint32_t>(
        cfg->getInt("common.ipc.max_frame_bytes", 10485760));
    config.ipc_config.receive_timeout_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.receive_timeout_ms", 60000));
    config.ipc_config.connect_timeout_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.connect_timeout_ms", 3000));
    config.ipc_config.listen_backlog =
        cfg->getInt("common.ipc.listen_backlog", 5);
    config.ipc_config.reconnect.initial_backoff_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.reconnect.initial_backoff_ms", 100));
    config.ipc_config.reconnect.max_backoff_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.reconnect.max_backoff_ms", 5000));
    config.ipc_config.reconnect.multiplier =
        cfg->getDouble("common.ipc.reconnect.multiplier", 2.0);
    
    // 创建 Store 实例
    tbox::framework::Store store("prov", store_root);
    
    // 创建并初始化 PROV 服务
    tbox::prov::ProvService service(store, config);
    auto result = service.initialize();
    
    if (result != tbox::prov::ErrorCode::SUCCESS) {
        tbox::prov::ProvLogAdapter::service().error("prov.service.initialization_failed",
            "Failed to initialize PROV service",
            {tbox::fw::log::Field("error_code", 
                tbox::fw::log::FieldValue::makeString(tbox::prov::error_code_to_string(result)))}
        );
        return 1;
    }
    
    tbox::prov::ProvLogAdapter::service().info("prov.service.initialized",
        "PROV service initialized successfully"
    );
    
    // 启动 IPC 服务器
    if (!service.start_ipc_server()) {
        tbox::prov::ProvLogAdapter::service().error("prov.service.ipc_start_failed",
            "Failed to start IPC server"
        );
        return 1;
    }
    
    tbox::prov::ProvLogAdapter::service().info("prov.service.ready",
        "PROV service is ready to accept IPC connections"
    );
    
    // 主循环（UDS协议处理由DIAG服务负责）
    while (!shutdown_requested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    tbox::prov::ProvLogAdapter::service().info("prov.service.shutting_down",
        "Shutting down PROV service...",
        {tbox::fw::log::Field("signal",
            tbox::fw::log::FieldValue::makeInt(static_cast<int>(received_signal)))}
    );

    // 显式停机：不依赖析构顺序，确保 IPC 线程回收与 socket 文件清理完成
    service.stop_ipc_server();

    tbox::prov::ProvLogAdapter::service().info("prov.service.stopped",
        "PROV service stopped"
    );

    return 0;
}
