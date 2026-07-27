#pragma once

#include <cstdint>

namespace tbox {
namespace prov {
namespace ipc {

// PROV IPC 方法 ID（业务协议契约，daemon 与 client 共享）
// 不重编号、不修改值，保持 wire 兼容。
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

// 默认 Unix Socket 路径
constexpr const char* DEFAULT_SOCKET_PATH = "/tmp/tbox-prov.sock";

} // namespace ipc
} // namespace prov
} // namespace tbox
