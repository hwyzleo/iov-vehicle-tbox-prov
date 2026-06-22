#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include "data_models.h"
#include "error_codes.h"

namespace tbox {
namespace prov {

class ProtectedStorage {
public:
    ProtectedStorage() = default;
    virtual ~ProtectedStorage() = default;

    // 初始化存储
    virtual ErrorCode initialize() = 0;
    
    // 读取车辆绑定信息
    virtual std::optional<VehicleBinding> read_vehicle_binding() = 0;
    
    // 写入车辆绑定信息
    virtual ErrorCode write_vehicle_binding(const VehicleBinding& binding) = 0;
    
    // 读取车辆配置
    virtual std::optional<VehicleConfig> read_vehicle_config() = 0;
    
    // 写入车辆配置
    virtual ErrorCode write_vehicle_config(const VehicleConfig& config) = 0;
    
    // 读取生产信息
    virtual std::optional<ProductionInfo> read_production_info() = 0;
    
    // 写入生产信息
    virtual ErrorCode write_production_info(const ProductionInfo& info) = 0;
    
    // 设置写保护
    virtual ErrorCode set_write_protection(bool locked) = 0;
    
    // 检查写保护状态
    virtual bool is_write_protected() = 0;
    
    // 清除所有数据
    virtual ErrorCode clear_all() = 0;
};

} // namespace prov
} // namespace tbox