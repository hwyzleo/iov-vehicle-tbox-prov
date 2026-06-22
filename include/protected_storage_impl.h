#pragma once

#include "protected_storage.h"
#include <string>
#include <mutex>

namespace tbox {
namespace prov {

class ProtectedStorageImpl : public ProtectedStorage {
public:
    ProtectedStorageImpl(const std::string& storage_path);
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
    std::string storage_path_;
    bool write_protected_ = false;
    std::mutex mutex_;
    
    // 文件路径
    std::string get_binding_file_path() const;
    std::string get_config_file_path() const;
    std::string get_production_info_file_path() const;
    std::string get_protection_file_path() const;
    
    // 文件操作
    bool write_to_file(const std::string& path, const std::string& data);
    std::string read_from_file(const std::string& path);
    bool file_exists(const std::string& path);
    bool create_directory(const std::string& path);
    
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