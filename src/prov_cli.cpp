#include "prov_service.h"
#include <iostream>
#include <string>

using namespace tbox::prov;

void print_usage() {
    std::cout << "PROV CLI Tool" << std::endl;
    std::cout << "Usage: prov_cli <command> [args...]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  init                    - Initialize PROV service" << std::endl;
    std::cout << "  write_vin <vin>         - Write VIN" << std::endl;
    std::cout << "  read_vin                - Read VIN" << std::endl;
    std::cout << "  read_uid                - Read ECU UID" << std::endl;
    std::cout << "  read_binding            - Read binding info" << std::endl;
    std::cout << "  get_state               - Get provision state" << std::endl;
    std::cout << "  write_config <hex>      - Write vehicle config" << std::endl;
    std::cout << "  authorize <vin>         - Authorize rewrite" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    // 配置 PROV 服务
    ProvServiceConfig config;
    config.storage_path = "/tmp/tbox/prov_test";
    config.enable_write_protection = true;
    config.max_retry_count = 3;

    // 创建并初始化 PROV 服务
    ProvService service(config);
    auto result = service.initialize();

    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize PROV service: " 
                  << error_code_to_string(result) << std::endl;
        return 1;
    }

    if (command == "init") {
        std::cout << "PROV service initialized successfully" << std::endl;
    }
    else if (command == "write_vin") {
        if (argc < 3) {
            std::cerr << "Usage: prov_cli write_vin <vin>" << std::endl;
            return 1;
        }
        std::string vin = argv[2];
        result = service.write_vin(vin);
        if (result == ErrorCode::SUCCESS) {
            std::cout << "VIN written successfully: " << vin << std::endl;
        } else {
            std::cerr << "Failed to write VIN: " << error_code_to_string(result) << std::endl;
            return 1;
        }
    }
    else if (command == "read_vin") {
        std::string vin = service.read_vin();
        if (vin.empty()) {
            std::cout << "VIN: (not written)" << std::endl;
        } else {
            std::cout << "VIN: " << vin << std::endl;
        }
    }
    else if (command == "read_uid") {
        // 主动读取 ECU UID（不依赖绑定）
        EcuUid uid_reader;
        auto uid_result = uid_reader.read_uid_detailed();
        if (uid_result.success) {
            std::cout << "ECU UID: " << uid_result.uid << std::endl;
            std::cout << "Source: ";
            if (uid_reader.is_se_hardware_present()) {
                std::cout << "SE hardware" << std::endl;
            } else if (uid_reader.is_test_environment()) {
                std::cout << "Config file (test environment)" << std::endl;
            } else {
                std::cout << "Unknown" << std::endl;
            }
        } else {
            std::cerr << "Failed to read ECU UID: " << uid_result.error_message << std::endl;
            std::cerr << "Error Code: " << error_code_to_string(uid_result.error_code) << std::endl;
            return 1;
        }
    }
    else if (command == "read_binding") {
        VehicleBinding binding = service.read_binding();
        std::cout << "Binding Info:" << std::endl;
        std::cout << "  VIN: " << (binding.vin.empty() ? "(not written)" : binding.vin) << std::endl;
        std::cout << "  ECU UID: " << (binding.ecu_uid.empty() ? "(not available)" : binding.ecu_uid) << std::endl;
        std::cout << "  State: ";
        switch (binding.state) {
            case ProvisionState::NONE: std::cout << "NONE"; break;
            case ProvisionState::VIN_WRITTEN: std::cout << "VIN_WRITTEN"; break;
            case ProvisionState::BOUND: std::cout << "BOUND"; break;
            case ProvisionState::FAILED: std::cout << "FAILED"; break;
        }
        std::cout << std::endl;
        std::cout << "  Locked: " << (binding.locked ? "true" : "false") << std::endl;
        std::cout << "  Retry Count: " << binding.retry_count << std::endl;
        std::cout << "  Rewrite Count: " << binding.rewrite_count << std::endl;
    }
    else if (command == "get_state") {
        ProvisionState state = service.get_provision_state();
        std::cout << "Provision State: ";
        switch (state) {
            case ProvisionState::NONE: std::cout << "NONE"; break;
            case ProvisionState::VIN_WRITTEN: std::cout << "VIN_WRITTEN"; break;
            case ProvisionState::BOUND: std::cout << "BOUND"; break;
            case ProvisionState::FAILED: std::cout << "FAILED"; break;
        }
        std::cout << std::endl;
    }
    else if (command == "write_config") {
        if (argc < 3) {
            std::cerr << "Usage: prov_cli write_config <hex_data>" << std::endl;
            return 1;
        }
        std::string hex_data = argv[2];
        std::vector<uint8_t> config_data(hex_data.begin(), hex_data.end());
        result = service.write_vehicle_config(config_data);
        if (result == ErrorCode::SUCCESS) {
            std::cout << "Vehicle config written successfully" << std::endl;
        } else {
            std::cerr << "Failed to write vehicle config: " << error_code_to_string(result) << std::endl;
            return 1;
        }
    }
    else if (command == "authorize") {
        if (argc < 3) {
            std::cerr << "Usage: prov_cli authorize <new_vin>" << std::endl;
            return 1;
        }
        std::string new_vin = argv[2];
        result = service.authorize_rewrite(new_vin);
        if (result == ErrorCode::SUCCESS) {
            std::cout << "Rewrite authorized successfully" << std::endl;
        } else {
            std::cerr << "Failed to authorize rewrite: " << error_code_to_string(result) << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        print_usage();
        return 1;
    }

    return 0;
}
