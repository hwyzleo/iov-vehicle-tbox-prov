#include "prov_ipc_dispatcher.h"
#include "prov_service.h"
#include "prov_log_adapter.h"
#include "prov_context.h"
#include "ipc_protocol.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace tbox {
namespace prov {

namespace {

// PROV IPC method IDs (与 ipc_protocol.h::MethodId 保持一致)
constexpr uint32_t METHOD_INITIALIZE          = 1;
constexpr uint32_t METHOD_READ_VIN            = 2;
constexpr uint32_t METHOD_READ_BINDING        = 3;
constexpr uint32_t METHOD_GET_PROVISION_STATE = 4;
constexpr uint32_t METHOD_WRITE_VIN           = 5;
constexpr uint32_t METHOD_WRITE_VEHICLE_CONFIG= 6;
constexpr uint32_t METHOD_WRITE_PRODUCTION_INFO=7;
constexpr uint32_t METHOD_AUTHORIZE_REWRITE   = 8;

// FW-0306: 未知 method 或 request handler 失败
constexpr int32_t FW_HANDLER_FAILED = 306;

std::string make_json_response(int32_t status, const nlohmann::json& payload) {
    nlohmann::json j = payload;
    j["status"] = status;
    return j.dump();
}

std::string extract_string_field(std::string_view params, const std::string& field) {
    try {
        auto j = nlohmann::json::parse(params);
        if (j.contains(field) && j[field].is_string()) {
            return j[field].get<std::string>();
        }
    } catch (...) {
        // 忽略解析错误，返回空
    }
    return "";
}

} // anonymous namespace

ProvIpcDispatcher::ProvIpcDispatcher(ProvService* service)
    : service_(service) {
}

std::string ProvIpcDispatcher::dispatch(uint32_t method_id,
                                        std::string_view params_json,
                                        int client_fd) {
    (void)client_fd;  // 当前无订阅需求，预留

    // 生成请求 ID 用于日志关联
    std::string request_id = generate_request_id();
    auto scope = make_context_scope(request_id, request_id, "");

    ProvLogAdapter::ipc().debug("prov.ipc.dispatch",
        "Dispatching request",
        {tbox::fw::log::Field("method_id", tbox::fw::log::FieldValue::makeInt(static_cast<int>(method_id))),
         tbox::fw::log::Field("params_bytes", tbox::fw::log::FieldValue::makeInt(static_cast<int>(params_json.size())))}
    );

    std::pair<int32_t, std::string> result{FW_HANDLER_FAILED, "{}"};

    try {
        switch (method_id) {
            case METHOD_INITIALIZE:
                result = handle_initialize();
                break;
            case METHOD_READ_VIN:
                result = handle_read_vin();
                break;
            case METHOD_READ_BINDING:
                result = handle_read_binding();
                break;
            case METHOD_GET_PROVISION_STATE:
                result = handle_get_provision_state();
                break;
            case METHOD_WRITE_VIN:
                result = handle_write_vin(params_json);
                break;
            case METHOD_WRITE_VEHICLE_CONFIG:
                result = handle_write_vehicle_config(params_json);
                break;
            case METHOD_WRITE_PRODUCTION_INFO:
                result = handle_write_production_info(params_json);
                break;
            case METHOD_AUTHORIZE_REWRITE:
                result = handle_authorize_rewrite(params_json);
                break;
            default:
                ProvLogAdapter::ipc().warn("prov.ipc.unknown_method",
                    "Unknown method ID",
                    {tbox::fw::log::Field("method_id", tbox::fw::log::FieldValue::makeInt(static_cast<int>(method_id)))}
                );
                result = {FW_HANDLER_FAILED, nlohmann::json({{"error", "Unknown method"}}).dump()};
                break;
        }
    } catch (const std::exception& e) {
        ProvLogAdapter::ipc().error("prov.ipc.dispatch_exception",
            "Exception in dispatch",
            {tbox::fw::log::Field("method_id", tbox::fw::log::FieldValue::makeInt(static_cast<int>(method_id))),
             tbox::fw::log::Field("what", tbox::fw::log::FieldValue::makeString(e.what()))}
        );
        result = {FW_HANDLER_FAILED, nlohmann::json({{"error", e.what()}}).dump()};
    } catch (...) {
        ProvLogAdapter::ipc().error("prov.ipc.dispatch_exception",
            "Unknown exception in dispatch",
            {tbox::fw::log::Field("method_id", tbox::fw::log::FieldValue::makeInt(static_cast<int>(method_id)))}
        );
        result = {FW_HANDLER_FAILED, nlohmann::json({{"error", "Unknown exception"}}).dump()};
    }

    // 构建最终 JSON：payload + status 字段
    try {
        auto payload = nlohmann::json::parse(result.second);
        return make_json_response(result.first, payload);
    } catch (...) {
        // payload 不是合法 JSON，包装为 error
        return make_json_response(result.first, nlohmann::json({{"error", "Invalid response"}}));
    }
}

std::pair<int32_t, std::string> ProvIpcDispatcher::handle_initialize() {
    auto result = service_->initialize();
    nlohmann::json j;
    j["success"] = (result == ErrorCode::SUCCESS);
    return {static_cast<int32_t>(result), j.dump()};
}

std::pair<int32_t, std::string> ProvIpcDispatcher::handle_read_vin() {
    std::string vin = service_->read_vin();
    nlohmann::json j;
    j["vin"] = vin;
    return {0, j.dump()};
}

std::pair<int32_t, std::string> ProvIpcDispatcher::handle_read_binding() {
    auto binding = service_->read_binding();
    nlohmann::json j;
    j["vin"] = binding.vin;
    j["ecu_uid"] = binding.ecu_uid;
    j["sn"] = binding.sn;  // TBOX SN（与 HSM ecu_uid 独立，可为空=不可用）
    j["state"] = static_cast<int>(binding.state);
    j["locked"] = binding.locked;
    return {0, j.dump()};
}

std::pair<int32_t, std::string> ProvIpcDispatcher::handle_get_provision_state() {
    auto state = service_->get_provision_state();
    nlohmann::json j;
    j["state"] = static_cast<int>(state);
    return {0, j.dump()};
}

std::pair<int32_t, std::string> ProvIpcDispatcher::handle_write_vin(std::string_view params) {
    std::string vin = extract_string_field(params, "vin");
    if (vin.empty()) {
        nlohmann::json j;
        j["success"] = false;
        j["error"] = "Missing vin parameter";
        return {static_cast<int32_t>(ErrorCode::INVALID_VIN_FORMAT), j.dump()};
    }
    auto result = service_->write_vin(vin);
    nlohmann::json j;
    j["success"] = (result == ErrorCode::SUCCESS);
    return {static_cast<int32_t>(result), j.dump()};
}

std::pair<int32_t, std::string> ProvIpcDispatcher::handle_write_vehicle_config(std::string_view params) {
    // 当前实现尚未支持配置写入
    nlohmann::json j;
    j["success"] = false;
    j["error"] = "Not implemented";
    return {static_cast<int32_t>(ErrorCode::CONFIG_WRITE_FAILED), j.dump()};
}

std::pair<int32_t, std::string> ProvIpcDispatcher::handle_write_production_info(std::string_view params) {
    // 当前实现尚未支持生产信息写入
    nlohmann::json j;
    j["success"] = false;
    j["error"] = "Not implemented";
    return {static_cast<int32_t>(ErrorCode::PRODUCTION_INFO_WRITE_FAILED), j.dump()};
}

std::pair<int32_t, std::string> ProvIpcDispatcher::handle_authorize_rewrite(std::string_view params) {
    std::string new_vin = extract_string_field(params, "vin");
    if (new_vin.empty()) {
        nlohmann::json j;
        j["success"] = false;
        j["error"] = "Missing vin parameter";
        return {static_cast<int32_t>(ErrorCode::INVALID_VIN_FORMAT), j.dump()};
    }
    auto result = service_->authorize_rewrite(new_vin);
    nlohmann::json j;
    j["success"] = (result == ErrorCode::SUCCESS);
    return {static_cast<int32_t>(result), j.dump()};
}

} // namespace prov
} // namespace tbox
