#include "ecu_uid.h"
#include "se_uid_provider.h"
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

// TBOX-PROV-DSN-CR-008 §14.3: 静态便捷接口转发到默认 SeUidProvider 实例，
// 保留既有单元测试（test_ecu_uid.cpp）与调用方平滑迁移。
std::string EcuUid::read_uid() {
    auto provider = createSeUidProvider();
    auto result = provider->readUidDetailed();
    return result.success ? result.uid : "";
}

UidReadResult EcuUid::read_uid_detailed() {
    auto provider = createSeUidProvider();
    return provider->readUidDetailed();
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
