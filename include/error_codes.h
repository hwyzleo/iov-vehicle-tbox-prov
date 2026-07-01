#pragma once

#include <cstdint>
#include <string>

namespace tbox {
namespace prov {

enum class ErrorCode : uint32_t {
    SUCCESS = 0,
    
    // PROV-1001: 安全访问未解锁 / 鉴权失败
    SECURITY_ACCESS_NOT_GRANTED = 1001,
    
    // PROV-1002: VIN格式非法
    INVALID_VIN_FORMAT = 1002,
    
    // PROV-1003: VIN写入失败
    VIN_WRITE_FAILED = 1003,
    
    // PROV-1004: 回读校验不一致
    READBACK_VERIFICATION_FAILED = 1004,
    
    // PROV-1005: 已绑定且VIN冲突（未授权改写）
    VIN_CONFLICT_UNAUTHORIZED = 1005,
    
    // PROV-1006: 配置/变量编码写入失败
    CONFIG_WRITE_FAILED = 1006,
    
    // PROV-1007: 生产信息写入失败
    PRODUCTION_INFO_WRITE_FAILED = 1007,
    
    // PROV-1008: SE UID 读取失败/超时（SE 在位）
    SE_UID_READ_FAILED = 1008,
    
    // PROV-1009: 无 SE（硬件缺失）且配置文件缺失/无对应 UID
    SE_MISSING_CONFIG_NOT_FOUND = 1009,
    
    // PROV-1010: 生产环境无 SE，禁止配置文件兜底
    SE_MISSING_PRODUCTION_FAIL_CLOSED = 1010,
    
    // 内部错误码
    INTERNAL_ERROR = 9999,
    STORAGE_ERROR = 9998,
    ECU_UID_READ_ERROR = 9997,
    INVALID_STATE = 9996
};

std::string error_code_to_string(ErrorCode code);
std::string error_code_to_description(ErrorCode code);

} // namespace prov
} // namespace tbox