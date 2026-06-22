#include "ecu_uid.h"
#include <iostream>
#include <fstream>
#include <sstream>

namespace tbox {
namespace prov {

std::string EcuUid::read_uid() {
    // 首先尝试从硬件读取
    std::string uid = read_from_hardware();
    if (!uid.empty()) {
        return uid;
    }
    
    // 如果硬件读取失败，尝试从SE读取
    uid = read_from_se();
    if (!uid.empty()) {
        return uid;
    }
    
    return "";
}

bool EcuUid::validate(const std::string& uid) {
    // 简单的UID格式验证（实际实现中可能需要更复杂的验证）
    return !uid.empty() && uid.length() >= 8 && uid.length() <= 32;
}

std::string EcuUid::get_read_error() {
    return "无法读取ECU UID";
}

std::string EcuUid::read_from_hardware() {
    // 实际实现中，这里会读取硬件序列号
    // 这里返回一个模拟的UID用于测试
    return "ECU123456789";
}

std::string EcuUid::read_from_se() {
    // 实际实现中，这里会从安全元件读取UID
    // 这里返回一个模拟的UID用于测试
    return "SE987654321";
}

} // namespace prov
} // namespace tbox