#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "tbox/prov/types.h"
#include "tbox/prov/errors.h"

namespace tbox {
namespace prov {

/// PROV 客户端 facade
///
/// 调用方（SEC / RSMS / TSP / DIAG 等）通过此接口与 PROV daemon 交互。
/// IPC 传输细节封装在库内部，对调用方不可见。
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
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace prov
} // namespace tbox
