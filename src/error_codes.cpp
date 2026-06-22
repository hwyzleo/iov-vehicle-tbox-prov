#include "error_codes.h"
#include <map>

namespace tbox {
namespace prov {

static const std::map<ErrorCode, std::string> error_code_strings = {
    {ErrorCode::SUCCESS, "SUCCESS"},
    {ErrorCode::SECURITY_ACCESS_NOT_GRANTED, "PROV-1001"},
    {ErrorCode::INVALID_VIN_FORMAT, "PROV-1002"},
    {ErrorCode::VIN_WRITE_FAILED, "PROV-1003"},
    {ErrorCode::READBACK_VERIFICATION_FAILED, "PROV-1004"},
    {ErrorCode::VIN_CONFLICT_UNAUTHORIZED, "PROV-1005"},
    {ErrorCode::CONFIG_WRITE_FAILED, "PROV-1006"},
    {ErrorCode::PRODUCTION_INFO_WRITE_FAILED, "PROV-1007"},
    {ErrorCode::INTERNAL_ERROR, "INTERNAL-9999"},
    {ErrorCode::STORAGE_ERROR, "INTERNAL-9998"},
    {ErrorCode::ECU_UID_READ_ERROR, "INTERNAL-9997"},
    {ErrorCode::INVALID_STATE, "INTERNAL-9996"}
};

static const std::map<ErrorCode, std::string> error_code_descriptions = {
    {ErrorCode::SUCCESS, "操作成功"},
    {ErrorCode::SECURITY_ACCESS_NOT_GRANTED, "安全访问未解锁/鉴权失败"},
    {ErrorCode::INVALID_VIN_FORMAT, "VIN格式非法"},
    {ErrorCode::VIN_WRITE_FAILED, "VIN写入失败"},
    {ErrorCode::READBACK_VERIFICATION_FAILED, "回读校验不一致"},
    {ErrorCode::VIN_CONFLICT_UNAUTHORIZED, "已绑定且VIN冲突（未授权改写）"},
    {ErrorCode::CONFIG_WRITE_FAILED, "配置/变量编码写入失败"},
    {ErrorCode::PRODUCTION_INFO_WRITE_FAILED, "生产信息写入失败"},
    {ErrorCode::INTERNAL_ERROR, "内部错误"},
    {ErrorCode::STORAGE_ERROR, "存储错误"},
    {ErrorCode::ECU_UID_READ_ERROR, "ECU UID读取错误"},
    {ErrorCode::INVALID_STATE, "无效状态"}
};

std::string error_code_to_string(ErrorCode code) {
    auto it = error_code_strings.find(code);
    if (it != error_code_strings.end()) {
        return it->second;
    }
    return "UNKNOWN";
}

std::string error_code_to_description(ErrorCode code) {
    auto it = error_code_descriptions.find(code);
    if (it != error_code_descriptions.end()) {
        return it->second;
    }
    return "未知错误";
}

} // namespace prov
} // namespace tbox