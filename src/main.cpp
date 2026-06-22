#include "prov_service.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> shutdown_requested(false);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        shutdown_requested = true;
    }
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "TBOX PROV Service Starting..." << std::endl;
    
    // 配置 PROV 服务
    tbox::prov::ProvServiceConfig config;
    config.storage_path = "/var/tbox/prov";
    config.enable_write_protection = true;
    config.max_retry_count = 3;
    
    // 创建并初始化 PROV 服务
    tbox::prov::ProvService service(config);
    auto result = service.initialize();
    
    if (result != tbox::prov::ErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize PROV service: " 
                  << tbox::prov::error_code_to_string(result) << std::endl;
        return 1;
    }
    
    std::cout << "PROV service initialized successfully" << std::endl;
    
    // 主循环（UDS协议处理由DIAG服务负责）
    while (!shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Shutting down PROV service..." << std::endl;
    
    return 0;
}