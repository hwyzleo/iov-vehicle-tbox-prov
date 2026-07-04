#pragma once

#include "protected_storage.h"
#include "framework_store.h"
#include <string>
#include <mutex>

namespace tbox {
namespace prov {

class ProtectedStorageImpl : public ProtectedStorage {
public:
    ProtectedStorageImpl(tbox::framework::Store& store);
    ~ProtectedStorageImpl() override = default;

    ErrorCode initialize() override;
    
    std::optional<VehicleBinding> read_vehicle_binding() override;
    ErrorCode write_vehicle_binding(const VehicleBinding& binding) override;
    
    std::optional<VehicleConfig> read_vehicle_config() override;
    ErrorCode write_vehicle_config(const VehicleConfig& config) override;
    
    std::optional<ProductionInfo> read_production_info() override;
    ErrorCode write_production_info(const ProductionInfo& info) override;
    
    ErrorCode set_write_protection(bool locked) override;
    bool is_write_protected() override;
    
    ErrorCode clear_all() override;

private:
    tbox::framework::Store& store_;
    bool write_protected_ = false;
    std::mutex mutex_;
    
    // 序列化/反序列化
    std::string serialize_binding(const VehicleBinding& binding);
    VehicleBinding deserialize_binding(const std::string& data);
    std::string serialize_config(const VehicleConfig& config);
    VehicleConfig deserialize_config(const std::string& data);
    std::string serialize_production_info(const ProductionInfo& info);
    ProductionInfo deserialize_production_info(const std::string& data);
};

} // namespace prov
} // namespace tbox