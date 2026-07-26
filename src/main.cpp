#include "prov_service.h"
#include "prov_log_adapter.h"
#include "framework_store.h"
#include "config.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> shutdown_requested(false);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        // 信号处理仍使用 stderr，因为 Logger 可能正在关闭
        std::cerr << "\n[signal] received signal " << signal << ", requesting shutdown" << std::endl;
        shutdown_requested = true;
    }
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
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
    while (!shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    tbox::prov::ProvLogAdapter::service().info("prov.service.shutting_down",
        "Shutting down PROV service..."
    );
    
    return 0;
}
