#include "ecu_uid.h"
#include "constants.h"
#include "error_codes.h"
#include <iostream>
#include <fstream>
#include <filesystem>

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
    // 实际实现中，这里会检查编译开关或环境变量
    // 例如：#ifdef TEST_ENVIRONMENT 或检查环境变量
    // 这里返回true用于测试
#ifdef TEST_ENVIRONMENT
    return true;
#else
    return false;
#endif
}

// 从配置文件读取UID
std::optional<std::string> EcuUid::read_uid_from_config_file() {
    // 首先尝试绝对路径
    auto result = parse_config_file(ConfigPath::TEST_UID_CONFIG);
    if (result.has_value()) {
        return result;
    }
    
    // 然后尝试相对路径
    result = parse_config_file(ConfigPath::TEST_UID_CONFIG_LOCAL);
    if (result.has_value()) {
        return result;
    }
    
    return std::nullopt;
}

// 解析配置文件
std::optional<std::string> EcuUid::parse_config_file(const std::string& file_path) {
    try {
        if (!std::filesystem::exists(file_path)) {
            return std::nullopt;
        }
        
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // 跳过注释和空行
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // 解析 key=value
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);
                
                // 去除首尾空格
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                if (key == "uid" && !value.empty()) {
                    return value;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading config file: " << e.what() << std::endl;
    }
    
    return std::nullopt;
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
        // 测试环境，尝试从配置文件读取
        auto config_uid = read_uid_from_config_file();
        if (config_uid.has_value()) {
            return UidReadResult(config_uid.value());
        }
        
        // 配置文件缺失或无对应UID，返回PROV-1009错误
        return UidReadResult(ErrorCode::SE_MISSING_CONFIG_NOT_FOUND, "无SE且配置文件缺失/无对应UID");
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
