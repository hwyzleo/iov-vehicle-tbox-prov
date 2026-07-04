#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tbox {
namespace prov {
namespace ipc {

// IPC 方法 ID
enum class MethodId : uint32_t {
    INITIALIZE = 1,
    READ_VIN = 2,
    READ_BINDING = 3,
    GET_PROVISION_STATE = 4,
    WRITE_VIN = 5,
    WRITE_VEHICLE_CONFIG = 6,
    WRITE_PRODUCTION_INFO = 7,
    AUTHORIZE_REWRITE = 8,
};

// IPC 请求头
struct RequestHeader {
    uint32_t method_id;      // 方法 ID
    uint32_t params_length;  // 参数数据长度
} __attribute__((packed));

// IPC 响应头
struct ResponseHeader {
    int32_t status_code;     // 状态码（0 表示成功）
    uint32_t data_length;    // 响应数据长度
} __attribute__((packed));

// 默认 Unix Socket 路径
constexpr const char* DEFAULT_SOCKET_PATH = "/tmp/tbox-prov.sock";

// 序列化/反序列化工具
class IpcSerializer {
public:
    // 序列化请求
    static std::vector<uint8_t> serialize_request(MethodId method, const std::string& params_json);
    
    // 反序列化请求
    static bool deserialize_request(const std::vector<uint8_t>& data, MethodId& method, std::string& params_json);
    
    // 序列化响应
    static std::vector<uint8_t> serialize_response(int32_t status_code, const std::string& response_json);
    
    // 反序列化响应
    static bool deserialize_response(const std::vector<uint8_t>& data, int32_t& status_code, std::string& response_json);
};

} // namespace ipc
} // namespace prov
} // namespace tbox