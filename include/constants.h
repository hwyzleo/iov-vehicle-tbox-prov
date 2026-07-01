#pragma once

#include <cstdint>
#include <string>

namespace tbox {
namespace prov {

// UDS服务ID
namespace UdsService {
    constexpr uint8_t DIAGNOSTIC_SESSION_CONTROL = 0x10;
    constexpr uint8_t SECURITY_ACCESS = 0x27;
    constexpr uint8_t AUTHENTICATION = 0x29;
    constexpr uint8_t READ_DATA_BY_IDENTIFIER = 0x22;
    constexpr uint8_t WRITE_DATA_BY_IDENTIFIER = 0x2E;
    constexpr uint8_t ROUTINE_CONTROL = 0x31;
}

// UDS安全级别
namespace UdsSecurityLevel {
    constexpr uint8_t LEVEL_0 = 0x00;
    constexpr uint8_t LEVEL_1 = 0x01;
    constexpr uint8_t LEVEL_27 = 0x27;
    constexpr uint8_t LEVEL_29 = 0x29;
}

// DID定义
namespace Did {
    constexpr uint16_t VIN = 0xF190;
    constexpr uint16_t VEHICLE_BINDING = 0xF191;
    constexpr uint16_t PROVISION_STATE = 0xF192;
    constexpr uint16_t VEHICLE_CONFIG = 0xF193;
    constexpr uint16_t PRODUCTION_INFO = 0xF194;
}

// RID定义
namespace Rid {
    constexpr uint16_t REWRITE_ROUTINE = 0xFF00;
}

// NVM分区名称
namespace NvmPartition {
    const std::string PROTECTED_STORAGE = "prov_protected";
}

// 配置文件路径
namespace ConfigPath {
    const std::string DEFAULT_CONFIG = "/etc/tbox/prov_config.yaml";
    const std::string TEST_UID_CONFIG = "/etc/tbox/prov_test.conf";
    const std::string TEST_UID_CONFIG_LOCAL = "./config/prov_test.conf";
}

} // namespace prov
} // namespace tbox