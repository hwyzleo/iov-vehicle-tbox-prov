//
// TBOX-PROV-DSN-CR-008 §10.1: 回滚验证用的旧式手写生命周期入口。
// 当 PROV_USE_FRAMEWORK_APPLICATION=OFF 时编译。不使用 hwyz::Application，
// 自行安装信号、初始化日志、装配 IPC 并维护运行循环。
// 验收后删除本文件与开关（CR-008 阶段 3）。
//
// 注意：ProvService 已按 §14.2 瘦身（移除 start_ipc_server/stop_ipc_server），
// 故本入口手动装配 dispatcher + ipc::Server，等价于 ProvApplication 的 IPC 部分。
//

#include "prov_service.h"
#include "prov_ipc_dispatcher.h"
#include "prov_log_adapter.h"
#include "se_uid_provider.h"
#include "tbox_sn_provider.h"
#include "framework_store.h"
#include "ipc.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>

static std::atomic<bool> shutdown_requested(false);
static volatile sig_atomic_t received_signal = 0;

extern "C" void signal_handler(int signum) {
    received_signal = signum;
    shutdown_requested.store(true, std::memory_order_relaxed);
    static const char msg[] = "\n[signal] shutdown requested\n";
    ssize_t ignored = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)ignored;
}

static bool install_signal_handlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGHUP);
    sa.sa_flags = SA_RESTART;

    const int shutdown_signals[] = {SIGINT, SIGTERM, SIGHUP};
    for (int sig : shutdown_signals) {
        if (sigaction(sig, &sa, nullptr) != 0) {
            std::cerr << "FATAL: sigaction failed for signal " << sig << std::endl;
            return false;
        }
    }

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
    if (!install_signal_handlers()) {
        return 1;
    }

    auto err = CONFIG_MANAGER.load("prov");
    if (err != hwyz::config::ConfigError::kOk) {
        auto info = CONFIG_MANAGER.getLastError();
        std::cerr << "FATAL: Config load failed: " << info.message << std::endl;
        return 1;
    }

    auto cfg = CONFIG_SNAPSHOT;
    tbox::fw::log::LogConfig log_config;
    log_config.level = tbox::fw::log::LogLevel::kInfo;
    log_config.strict = true;

    std::string log_level_str = cfg->getString("common.log.level", "info");
    if (log_level_str == "trace") log_config.level = tbox::fw::log::LogLevel::kTrace;
    else if (log_level_str == "debug") log_config.level = tbox::fw::log::LogLevel::kDebug;
    else if (log_level_str == "info") log_config.level = tbox::fw::log::LogLevel::kInfo;
    else if (log_level_str == "warn") log_config.level = tbox::fw::log::LogLevel::kWarn;
    else if (log_level_str == "error") log_config.level = tbox::fw::log::LogLevel::kError;

    auto log_result = tbox::prov::ProvLogAdapter::init("prov", log_config);
    if (log_result.error != tbox::fw::log::LogError::kOk) {
        std::cerr << "FATAL: Logger init failed: " << log_result.error_message << std::endl;
        return 1;
    }

    tbox::prov::ProvLogAdapter::service().info("prov.service.starting",
        "TBOX PROV Service Starting (legacy lifecycle)");

    std::string store_root = cfg->getString("common.store.root", "/var/tbox");
    std::string ipc_socket_path = cfg->getString("prov.ipc.socket_path", "/tmp/tbox-prov.sock");

    tbox::fw::ipc::IpcConfig ipc_config;
    ipc_config.max_frame_bytes = static_cast<uint32_t>(
        cfg->getInt("common.ipc.max_frame_bytes", 10485760));
    ipc_config.receive_timeout_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.receive_timeout_ms", 60000));
    ipc_config.connect_timeout_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.connect_timeout_ms", 3000));
    ipc_config.listen_backlog = cfg->getInt("common.ipc.listen_backlog", 5);
    ipc_config.reconnect.initial_backoff_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.reconnect.initial_backoff_ms", 100));
    ipc_config.reconnect.max_backoff_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.reconnect.max_backoff_ms", 5000));
    ipc_config.reconnect.multiplier =
        cfg->getDouble("common.ipc.reconnect.multiplier", 2.0);

    tbox::prov::ProvServiceConfig svc_config;
    svc_config.enable_write_protection = cfg->getBool("storage.enable_write_protection", true);
    svc_config.max_retry_count = static_cast<uint32_t>(cfg->getInt("storage.max_retry_count", 3));

    tbox::framework::Store store("prov", store_root);
    auto uid_provider = tbox::prov::createSeUidProvider();
    auto sn_provider = tbox::prov::createTboxSnProvider();

    tbox::prov::ProvService service(store, svc_config, *uid_provider, *sn_provider);
    if (service.initialize() != tbox::prov::ErrorCode::SUCCESS) {
        tbox::prov::ProvLogAdapter::service().error("prov.service.initialization_failed",
            "Failed to initialize PROV service");
        return 1;
    }

    // 手动装配 dispatcher + framework-ipc Server（等价 ProvApplication 的 IPC 部分）
    auto dispatcher = std::make_unique<tbox::prov::ProvIpcDispatcher>(&service);
    auto ipc_server = std::make_unique<tbox::fw::ipc::Server>(ipc_socket_path, ipc_config);
    auto* dispatcher_ptr = dispatcher.get();
    auto request_handler = [dispatcher_ptr](uint32_t method_id,
                                            std::string_view params_json,
                                            int client_fd) -> std::string {
        return dispatcher_ptr->dispatch(method_id, params_json, client_fd);
    };
    if (!ipc_server->start(std::move(request_handler), {})) {
        tbox::prov::ProvLogAdapter::service().error("prov.service.ipc_start_failed",
            "Failed to start IPC server");
        return 1;
    }

    tbox::prov::ProvLogAdapter::service().info("prov.service.ready",
        "PROV service is ready to accept IPC connections");

    while (!shutdown_requested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    tbox::prov::ProvLogAdapter::service().info("prov.service.shutting_down",
        "Shutting down PROV service...",
        {tbox::fw::log::Field("signal",
            tbox::fw::log::FieldValue::makeInt(static_cast<int>(received_signal)))});

    ipc_server->stop();
    dispatcher.reset();

    tbox::prov::ProvLogAdapter::service().info("prov.service.stopped",
        "PROV service stopped");
    return 0;
}
