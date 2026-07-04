#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "data_models.h"
#include "error_codes.h"

namespace tbox {
namespace prov {

class ProvClient {
public:
    ProvClient(const std::string& socket_path = "/tmp/tbox-prov.sock");
    ~ProvClient();
    
    // 连接到 PROV 服务
    bool connect();
    
    // 断开连接
    void disconnect();
    
    // 检查是否已连接
    bool is_connected() const;
    
    // 初始化服务
    ErrorCode initialize();
    
    // 读取 VIN
    std::string read_vin();
    
    // 读取绑定信息
    VehicleBinding read_binding();
    
    // 获取个性化状态
    ProvisionState get_provision_state();
    
    // 写入 VIN（供 DIAG 调用）
    ErrorCode write_vin(const std::string& vin);
    
    // 写入车辆配置
    ErrorCode write_vehicle_config(const std::vector<uint8_t>& config_data);
    
    // 写入生产信息
    ErrorCode write_production_info(const ProductionInfo& info);
    
    // 授权重写
    ErrorCode authorize_rewrite(const std::string& new_vin);
    
private:
    std::string socket_path_;
    int socket_fd_;
    bool connected_;
    
    // 发送请求并接收响应
    bool send_request(uint32_t method_id, const std::string& params_json, 
                     int32_t& status_code, std::string& response_json);
    
    // 序列化请求
    std::vector<uint8_t> serialize_request(uint32_t method_id, const std::string& params_json);
    
    // 反序列化响应
    bool deserialize_response(const std::vector<uint8_t>& data, int32_t& status_code, std::string& response_json);
};

} // namespace prov
} // namespace tbox