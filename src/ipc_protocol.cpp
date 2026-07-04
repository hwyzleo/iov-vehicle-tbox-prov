#include "ipc_protocol.h"
#include <cstring>

namespace tbox {
namespace prov {
namespace ipc {

std::vector<uint8_t> IpcSerializer::serialize_request(MethodId method, const std::string& params_json) {
    RequestHeader header;
    header.method_id = static_cast<uint32_t>(method);
    header.params_length = static_cast<uint32_t>(params_json.size());
    
    std::vector<uint8_t> result(sizeof(RequestHeader) + params_json.size());
    std::memcpy(result.data(), &header, sizeof(RequestHeader));
    std::memcpy(result.data() + sizeof(RequestHeader), params_json.data(), params_json.size());
    
    return result;
}

bool IpcSerializer::deserialize_request(const std::vector<uint8_t>& data, MethodId& method, std::string& params_json) {
    if (data.size() < sizeof(RequestHeader)) {
        return false;
    }
    
    RequestHeader header;
    std::memcpy(&header, data.data(), sizeof(RequestHeader));
    
    if (data.size() < sizeof(RequestHeader) + header.params_length) {
        return false;
    }
    
    method = static_cast<MethodId>(header.method_id);
    params_json = std::string(reinterpret_cast<const char*>(data.data() + sizeof(RequestHeader)), header.params_length);
    
    return true;
}

std::vector<uint8_t> IpcSerializer::serialize_response(int32_t status_code, const std::string& response_json) {
    ResponseHeader header;
    header.status_code = status_code;
    header.data_length = static_cast<uint32_t>(response_json.size());
    
    std::vector<uint8_t> result(sizeof(ResponseHeader) + response_json.size());
    std::memcpy(result.data(), &header, sizeof(ResponseHeader));
    std::memcpy(result.data() + sizeof(ResponseHeader), response_json.data(), response_json.size());
    
    return result;
}

bool IpcSerializer::deserialize_response(const std::vector<uint8_t>& data, int32_t& status_code, std::string& response_json) {
    if (data.size() < sizeof(ResponseHeader)) {
        return false;
    }
    
    ResponseHeader header;
    std::memcpy(&header, data.data(), sizeof(ResponseHeader));
    
    if (data.size() < sizeof(ResponseHeader) + header.data_length) {
        return false;
    }
    
    status_code = header.status_code;
    response_json = std::string(reinterpret_cast<const char*>(data.data() + sizeof(ResponseHeader)), header.data_length);
    
    return true;
}

} // namespace ipc
} // namespace prov
} // namespace tbox