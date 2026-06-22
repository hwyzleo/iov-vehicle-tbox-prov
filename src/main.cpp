#include <iostream>
#include <memory>
#include "prov_service.h"
#include "error_codes.h"

int main() {
    std::cout << "TBOX PROV Service Starting..." << std::endl;
    
    tbox::prov::ProvServiceConfig config;
    config.storage_path = "/var/tbox/prov";
    config.enable_write_protection = true;
    config.max_retry_count = 3;
    
    auto service = std::make_unique<tbox::prov::ProvService>(config);
    
    auto result = service->initialize();
    if (result != tbox::prov::ErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize PROV service: " 
                  << tbox::prov::error_code_to_string(result) << std::endl;
        return 1;
    }
    
    std::cout << "TBOX PROV Service initialized successfully" << std::endl;
    
    // 主循环（实际实现中会包含UDS服务监听）
    std::cout << "PROV service running..." << std::endl;
    
    return 0;
}