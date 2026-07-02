#include "ecu_uid.h"
#include "config.h"
#include "error_codes.h"
#include <iostream>

namespace tbox {
namespace prov {

// SE硬件检测（实际实现中应检测SE硬件是否存在）
bool EcuUid::is_se_hardware_present() {
    // 测试配置文件路径：return false;
    // 生产环境：return true;
    return false;  // 临时改为 false，测试配置文件路径
}

// 检查是否为测试环境
bool EcuUid::is_test_environment() {
#ifdef TEST_ENVIRONMENT
    return true;
#else
    return false;
#endif
}

// 从配置读取UID（使用框架ConfigManager）
std::optional<std::string> EcuUid::read_uid_from_config() {
    auto cfg = CONFIG_SNAPSHOT;
    if (!cfg) {
        return std::nullopt;
    }

    if (!cfg->has("ecu.uid")) {
        return std::nullopt;
    }

    std::string uid = cfg->getString("ecu.uid");
    if (uid.empty()) {
        return std::nullopt;
    }

    return uid;
}

// 从SE读取UID
std::string EcuUid::read_from_se() {
    // 实际实现中，这里会从安全元件读取UID
    // 这里返回一个模拟的UID用于测试
    return "SE987654321";
}

std::string EcuUid::read_uid() {
    auto result = read_uid_detailed();
    return result.success ? result.uid : "";
}

UidReadResult EcuUid::read_uid_detailed() {
    // 1. 优先尝试从SE读取
    if (is_se_hardware_present()) {
        // SE在位，尝试读取
        std::string uid = read_from_se();
        if (!uid.empty()) {
            return UidReadResult(uid);
        }
        
        // SE读取失败，返回PROV-1008错误
        return UidReadResult(ErrorCode::SE_UID_READ_FAILED, "SE UID读取失败/超时");
    }
    
    // 2. SE硬件缺失
    if (is_test_environment()) {
        // 测试环境，尝试从配置读取（使用框架ConfigManager）
        auto config_uid = read_uid_from_config();
        if (config_uid.has_value()) {
            return UidReadResult(config_uid.value());
        }
        
        // 配置缺失或无对应UID，返回PROV-1009错误
        return UidReadResult(ErrorCode::SE_MISSING_CONFIG_NOT_FOUND, "无SE且配置缺失/无对应UID");
    }
    
    // 3. 生产环境无SE，返回PROV-1010错误（fail-closed）
    return UidReadResult(ErrorCode::SE_MISSING_PRODUCTION_FAIL_CLOSED, "生产环境无SE，禁止配置文件兜底");
}

bool EcuUid::validate(const std::string& uid) {
    // 简单的UID格式验证（实际实现中可能需要更复杂的验证）
    return !uid.empty() && uid.length() >= 8 && uid.length() <= 32;
}

std::string EcuUid::get_read_error() {
    return "无法读取ECU UID";
}

} // namespace prov
} // namespace tbox
