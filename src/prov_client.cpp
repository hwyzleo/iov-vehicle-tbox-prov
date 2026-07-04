#include "prov_client.h"
#include "ipc_protocol.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

namespace tbox {
namespace prov {

ProvClient::ProvClient(const std::string& socket_path) 
    : socket_path_(socket_path), socket_fd_(-1), connected_(false) {
}

ProvClient::~ProvClient() {
    disconnect();
}

bool ProvClient::connect() {
    if (connected_) {
        return true;
    }
    
    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (::connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to connect to PROV service: " << strerror(errno) << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    return true;
}

void ProvClient::disconnect() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
}

bool ProvClient::is_connected() const {
    return connected_;
}

ErrorCode ProvClient::initialize() {
    int32_t status_code;
    std::string response_json;
    
    if (!send_request(static_cast<uint32_t>(ipc::MethodId::INITIALIZE), "{}", status_code, response_json)) {
        return ErrorCode::INVALID_STATE;
    }
    
    return static_cast<ErrorCode>(status_code);
}

std::string ProvClient::read_vin() {
    int32_t status_code;
    std::string response_json;
    
    if (!send_request(static_cast<uint32_t>(ipc::MethodId::READ_VIN), "{}", status_code, response_json)) {
        return "";
    }
    
    // 解析响应 JSON
    // 简单实现：假设响应格式为 {"vin":"..."}
    size_t pos = response_json.find("\"vin\":\"");
    if (pos != std::string::npos) {
        pos += 7;
        size_t end = response_json.find("\"", pos);
        if (end != std::string::npos) {
            return response_json.substr(pos, end - pos);
        }
    }
    
    return "";
}

VehicleBinding ProvClient::read_binding() {
    int32_t status_code;
    std::string response_json;
    VehicleBinding binding;
    
    if (!send_request(static_cast<uint32_t>(ipc::MethodId::READ_BINDING), "{}", status_code, response_json)) {
        return binding;
    }
    
    // 解析响应 JSON
    // 简单实现：实际应使用 JSON 解析库
    // 这里只是示例
    return binding;
}

ProvisionState ProvClient::get_provision_state() {
    int32_t status_code;
    std::string response_json;
    
    if (!send_request(static_cast<uint32_t>(ipc::MethodId::GET_PROVISION_STATE), "{}", status_code, response_json)) {
        return ProvisionState::NONE;
    }
    
    // 解析响应 JSON
    // 简单实现：假设响应格式为 {"state":0}
    size_t pos = response_json.find("\"state\":");
    if (pos != std::string::npos) {
        pos += 8;
        size_t end = response_json.find("}", pos);
        if (end != std::string::npos) {
            int state = std::stoi(response_json.substr(pos, end - pos));
            return static_cast<ProvisionState>(state);
        }
    }
    
    return ProvisionState::NONE;
}

ErrorCode ProvClient::write_vin(const std::string& vin) {
    int32_t status_code;
    std::string response_json;
    std::string params_json = "{\"vin\":\"" + vin + "\"}";
    
    if (!send_request(static_cast<uint32_t>(ipc::MethodId::WRITE_VIN), params_json, status_code, response_json)) {
        return ErrorCode::INVALID_STATE;
    }
    
    return static_cast<ErrorCode>(status_code);
}

ErrorCode ProvClient::write_vehicle_config(const std::vector<uint8_t>& config_data) {
    // 简化实现：实际应将字节数组转换为 JSON 数组
    int32_t status_code;
    std::string response_json;
    std::string params_json = "{\"config_data\":[]}";
    
    if (!send_request(static_cast<uint32_t>(ipc::MethodId::WRITE_VEHICLE_CONFIG), params_json, status_code, response_json)) {
        return ErrorCode::INVALID_STATE;
    }
    
    return static_cast<ErrorCode>(status_code);
}

ErrorCode ProvClient::write_production_info(const ProductionInfo& info) {
    // 简化实现：实际应将 ProductionInfo 转换为 JSON
    int32_t status_code;
    std::string response_json;
    std::string params_json = "{}";
    
    if (!send_request(static_cast<uint32_t>(ipc::MethodId::WRITE_PRODUCTION_INFO), params_json, status_code, response_json)) {
        return ErrorCode::INVALID_STATE;
    }
    
    return static_cast<ErrorCode>(status_code);
}

ErrorCode ProvClient::authorize_rewrite(const std::string& new_vin) {
    int32_t status_code;
    std::string response_json;
    std::string params_json = "{\"vin\":\"" + new_vin + "\"}";
    
    if (!send_request(static_cast<uint32_t>(ipc::MethodId::AUTHORIZE_REWRITE), params_json, status_code, response_json)) {
        return ErrorCode::INVALID_STATE;
    }
    
    return static_cast<ErrorCode>(status_code);
}

bool ProvClient::send_request(uint32_t method_id, const std::string& params_json, 
                             int32_t& status_code, std::string& response_json) {
    if (!connected_) {
        return false;
    }
    
    // 序列化请求
    auto request_data = serialize_request(method_id, params_json);
    
    // 发送请求
    size_t total_sent = 0;
    while (total_sent < request_data.size()) {
        ssize_t bytes_sent = send(socket_fd_, request_data.data() + total_sent, request_data.size() - total_sent, 0);
        if (bytes_sent <= 0) {
            std::cerr << "Failed to send request: " << strerror(errno) << std::endl;
            disconnect();
            return false;
        }
        total_sent += bytes_sent;
    }
    
    // 接收响应头（循环读取确保完整）
    ipc::ResponseHeader header;
    memset(&header, 0, sizeof(header));
    size_t header_received = 0;
    while (header_received < sizeof(header)) {
        ssize_t bytes_read = recv(socket_fd_, reinterpret_cast<uint8_t*>(&header) + header_received, sizeof(header) - header_received, 0);
        if (bytes_read <= 0) {
            std::cerr << "Failed to receive response header: " << strerror(errno) << std::endl;
            disconnect();
            return false;
        }
        header_received += bytes_read;
    }
    
    // 合理性检查
    if (header.data_length > 10 * 1024 * 1024) {  // 最大 10MB
        std::cerr << "Response too large: " << header.data_length << std::endl;
        disconnect();
        return false;
    }
    
    // 接收响应数据（循环读取确保完整）
    std::vector<uint8_t> response_data(sizeof(header) + header.data_length);
    memcpy(response_data.data(), &header, sizeof(header));
    
    size_t data_received = 0;
    while (data_received < header.data_length) {
        ssize_t bytes_read = recv(socket_fd_, response_data.data() + sizeof(header) + data_received, header.data_length - data_received, 0);
        if (bytes_read <= 0) {
            std::cerr << "Failed to receive response data: " << strerror(errno) << std::endl;
            disconnect();
            return false;
        }
        data_received += bytes_read;
    }
    
    // 反序列化响应
    return deserialize_response(response_data, status_code, response_json);
}

std::vector<uint8_t> ProvClient::serialize_request(uint32_t method_id, const std::string& params_json) {
    ipc::RequestHeader header;
    header.method_id = method_id;
    header.params_length = static_cast<uint32_t>(params_json.size());
    
    std::vector<uint8_t> result(sizeof(header) + params_json.size());
    memcpy(result.data(), &header, sizeof(header));
    memcpy(result.data() + sizeof(header), params_json.data(), params_json.size());
    
    return result;
}

bool ProvClient::deserialize_response(const std::vector<uint8_t>& data, int32_t& status_code, std::string& response_json) {
    if (data.size() < sizeof(ipc::ResponseHeader)) {
        return false;
    }
    
    ipc::ResponseHeader header;
    memcpy(&header, data.data(), sizeof(header));
    
    if (data.size() < sizeof(header) + header.data_length) {
        return false;
    }
    
    status_code = header.status_code;
    response_json = std::string(reinterpret_cast<const char*>(data.data() + sizeof(header)), header.data_length);
    
    return true;
}

} // namespace prov
} // namespace tbox