#pragma once

#include <string>
#include <cstdint>

namespace tbox {
namespace prov {

class EcuUid {
public:
    EcuUid() = default;
    ~EcuUid() = default;

    // 读取ECU UID（硬件序列号）
    static std::string read_uid();
    
    // 验证ECU UID格式
    static bool validate(const std::string& uid);
    
    // 获取UID读取错误信息
    static std::string get_read_error();

private:
    // 从硬件读取UID的具体实现
    static std::string read_from_hardware();
    
    // 从SE读取UID的具体实现
    static std::string read_from_se();
};

} // namespace prov
} // namespace tbox