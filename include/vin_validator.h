#pragma once

#include <string>
#include <cstdint>

namespace tbox {
namespace prov {

class VinValidator {
public:
    VinValidator() = default;
    ~VinValidator() = default;

    // 验证VIN格式是否合法
    static bool validate(const std::string& vin);
    
    // 获取VIN校验错误信息
    static std::string get_validation_error(const std::string& vin);

private:
    // 检查VIN长度
    static bool check_length(const std::string& vin);
    
    // 检查字符集（排除I、O、Q）
    static bool check_charset(const std::string& vin);
    
    // 检查校验位
    static bool check_digit(const std::string& vin);
    
    // 计算校验位
    static char calculate_check_digit(const std::string& vin);
    
    // 字符转数值
    static int char_to_value(char c);
};

} // namespace prov
} // namespace tbox