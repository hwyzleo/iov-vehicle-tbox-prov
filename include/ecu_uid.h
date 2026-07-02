#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include "error_codes.h"

namespace tbox {
namespace prov {

// UID读取结果结构
struct UidReadResult {
    std::string uid;
    bool success;
    ErrorCode error_code;
    std::string error_message;
    
    UidReadResult() : success(false), error_code(ErrorCode::SUCCESS) {}
    UidReadResult(const std::string& uid) : uid(uid), success(true), error_code(ErrorCode::SUCCESS) {}
    UidReadResult(ErrorCode code, const std::string& error) : success(false), error_code(code), error_message(error) {}
};

class EcuUid {
public:
    EcuUid() = default;
    ~EcuUid() = default;

    // 读取ECU UID（实现SE优先策略）
    static std::string read_uid();
    
    // 读取ECU UID（返回详细结果）
    static UidReadResult read_uid_detailed();
    
    // 验证ECU UID格式
    static bool validate(const std::string& uid);
    
    // 获取UID读取错误信息
    static std::string get_read_error();
    
    // 检查SE硬件是否在位
    static bool is_se_hardware_present();
    
    // 检查是否为测试环境
    static bool is_test_environment();
    
    // 从配置读取UID（仅测试环境，使用框架ConfigManager）
    static std::optional<std::string> read_uid_from_config();

private:
    // 从SE读取UID的具体实现
    static std::string read_from_se();
};

} // namespace prov
} // namespace tbox