#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace tbox {
namespace prov {

class ProvService;

/// PROV IPC 请求分发适配器
///
/// 将 framework-ipc 的 RequestHandler 签名适配到 PROV 业务 handler。
/// 只负责 JSON 解码/编码、调用业务 handler 及业务状态映射，
/// 不复制 socket 逻辑（由 framework-ipc Server 负责）。
///
/// 响应 JSON 中嵌入 `status` 字段（PROV-10xx 业务状态码），
/// framework 在 ResponseHeader.status_code 中写入 0（传输成功）或 FW-03xx（传输失败）。
class ProvIpcDispatcher {
public:
    explicit ProvIpcDispatcher(ProvService* service);

    /// framework RequestHandler 回调入口
    /// @param method_id   PROV method ID
    /// @param params_json 请求参数 JSON
    /// @param client_fd   客户端 fd（用于订阅管理）
    /// @return 响应 JSON 字符串
    std::string dispatch(uint32_t method_id,
                         std::string_view params_json,
                         int client_fd);

private:
    ProvService* service_;

    // 各 method handler，返回 (status_code, response_json)
    std::pair<int32_t, std::string> handle_initialize();
    std::pair<int32_t, std::string> handle_read_vin();
    std::pair<int32_t, std::string> handle_read_binding();
    std::pair<int32_t, std::string> handle_get_provision_state();
    std::pair<int32_t, std::string> handle_write_vin(std::string_view params);
    std::pair<int32_t, std::string> handle_write_vehicle_config(std::string_view params);
    std::pair<int32_t, std::string> handle_write_production_info(std::string_view params);
    std::pair<int32_t, std::string> handle_authorize_rewrite(std::string_view params);
};

} // namespace prov
} // namespace tbox
