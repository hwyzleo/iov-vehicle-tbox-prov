#include "tbox/prov/client.h"
#include "ipc_protocol.h"
#include "prov_log_adapter.h"
#include "ipc.h"
#include "ipc_types.h"
#include <nlohmann/json.hpp>

namespace tbox {
namespace prov {

// ============================================================
// ProvClient::Impl
// ============================================================
class ProvClient::Impl {
public:
    explicit Impl(const std::string& socket_path)
        : socket_path_(socket_path),
          fw_client_(std::make_unique<tbox::fw::ipc::Client>(socket_path_)) {
    }

    ~Impl() {
        disconnect();
    }

    bool connect() {
        return fw_client_->connect();
    }

    void disconnect() {
        fw_client_->disconnect();
    }

    bool is_connected() const {
        return fw_client_->is_connected();
    }

    /// 发送请求并接收响应
    /// @return (ok, biz_status_code, response_json)
    std::tuple<bool, int32_t, std::string>
    send_request(uint32_t method_id, const std::string& params_json) {
        auto [fw_status, response_json] = fw_client_->call(method_id, params_json);

        if (fw_status < 0) {
            // 客户端传输错误（已含一次重连重试）
            ProvLogAdapter::ipc().error("prov.ipc.client_transport_error",
                "IPC transport error",
                {tbox::fw::log::Field("method_id", tbox::fw::log::FieldValue::makeInt(static_cast<int>(method_id))),
                 tbox::fw::log::Field("fw_status", tbox::fw::log::FieldValue::makeInt(fw_status))}
            );
            return {false, 0, ""};
        }

        if (fw_status > 0) {
            // 服务端传输/handler 错误（FW-03xx）
            ProvLogAdapter::ipc().error("prov.ipc.server_error",
                "IPC server error",
                {tbox::fw::log::Field("method_id", tbox::fw::log::FieldValue::makeInt(static_cast<int>(method_id))),
                 tbox::fw::log::Field("fw_status", tbox::fw::log::FieldValue::makeInt(fw_status))}
            );
            return {false, 0, ""};
        }

        // fw_status == 0: 传输成功，从 JSON 提取业务状态码
        int32_t biz_status = 0;
        try {
            auto j = nlohmann::json::parse(response_json);
            biz_status = j.value("status", 0);
        } catch (...) {
            // JSON 解析失败，保持 biz_status=0
        }
        return {true, biz_status, response_json};
    }

private:
    std::string socket_path_;
    std::unique_ptr<tbox::fw::ipc::Client> fw_client_;
};

// ============================================================
// ProvClient facade
// ============================================================
ProvClient::ProvClient(const std::string& socket_path)
    : impl_(std::make_unique<Impl>(socket_path)) {
}

ProvClient::~ProvClient() = default;

bool ProvClient::connect() {
    return impl_->connect();
}

void ProvClient::disconnect() {
    impl_->disconnect();
}

bool ProvClient::is_connected() const {
    return impl_->is_connected();
}

ErrorCode ProvClient::initialize() {
    auto [ok, status, json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::INITIALIZE), "{}");
    if (!ok) return ErrorCode::INVALID_STATE;
    return static_cast<ErrorCode>(status);
}

std::string ProvClient::read_vin() {
    auto [ok, status, response_json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::READ_VIN), "{}");
    if (!ok) return "";

    try {
        auto j = nlohmann::json::parse(response_json);
        return j.value("vin", "");
    } catch (...) {
        return "";
    }
}

VehicleBinding ProvClient::read_binding() {
    VehicleBinding binding;
    auto [ok, status, response_json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::READ_BINDING), "{}");
    if (!ok) return binding;

    try {
        auto j = nlohmann::json::parse(response_json);
        binding.vin = j.value("vin", "");
        binding.ecu_uid = j.value("ecu_uid", "");
        // 新 Client 解析旧 Server 响应时若缺少 sn，留空表示 SN 不可用；
        // 不得复制 ecu_uid 作为 sn 默认值
        binding.sn = j.value("sn", "");
        binding.state = static_cast<ProvisionState>(j.value("state", 0));
        binding.locked = j.value("locked", false);
    } catch (...) {
    }
    return binding;
}

ProvisionState ProvClient::get_provision_state() {
    auto [ok, status, response_json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::GET_PROVISION_STATE), "{}");
    if (!ok) return ProvisionState::NONE;

    try {
        auto j = nlohmann::json::parse(response_json);
        return static_cast<ProvisionState>(j.value("state", 0));
    } catch (...) {
        return ProvisionState::NONE;
    }
}

ErrorCode ProvClient::write_vin(const std::string& vin) {
    std::string params = nlohmann::json({{"vin", vin}}).dump();
    auto [ok, status, json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::WRITE_VIN), params);
    if (!ok) return ErrorCode::INVALID_STATE;
    return static_cast<ErrorCode>(status);
}

ErrorCode ProvClient::write_vehicle_config(const std::vector<uint8_t>& config_data) {
    // 将字节数组转为 base64 或 JSON 数组（简化：空数组）
    std::string params = nlohmann::json({{"config_data", nlohmann::json::array()}}).dump();
    auto [ok, status, json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::WRITE_VEHICLE_CONFIG), params);
    if (!ok) return ErrorCode::INVALID_STATE;
    return static_cast<ErrorCode>(status);
}

ErrorCode ProvClient::write_production_info(const ProductionInfo& info) {
    nlohmann::json j;
    j["production_date"] = info.production_date;
    j["batch_num"] = info.batch_num;
    j["station_id"] = info.station_id;
    auto [ok, status, json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::WRITE_PRODUCTION_INFO), j.dump());
    if (!ok) return ErrorCode::INVALID_STATE;
    return static_cast<ErrorCode>(status);
}

ErrorCode ProvClient::authorize_rewrite(const std::string& new_vin) {
    std::string params = nlohmann::json({{"vin", new_vin}}).dump();
    auto [ok, status, json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::AUTHORIZE_REWRITE), params);
    if (!ok) return ErrorCode::INVALID_STATE;
    return static_cast<ErrorCode>(status);
}

} // namespace prov
} // namespace tbox
