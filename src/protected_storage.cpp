#include "protected_storage_impl.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

namespace tbox {
namespace prov {

ProtectedStorageImpl::ProtectedStorageImpl(tbox::framework::Store& store) 
    : store_(store) {
}

ErrorCode ProtectedStorageImpl::initialize() {
    try {
        if (!store_.initialize()) {
            return ErrorCode::STORAGE_ERROR;
        }
        
        // 检查写保护状态
        auto protection_data = store_.load<std::string>("protection");
        if (protection_data.has_value()) {
            write_protected_ = (protection_data.value() == "1");
        }
        
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Storage initialization failed: " << e.what() << std::endl;
        return ErrorCode::STORAGE_ERROR;
    }
}

std::optional<VehicleBinding> ProtectedStorageImpl::read_vehicle_binding() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "[storage] read_vehicle_binding called" << std::endl;
    try {
        auto data = store_.load<std::string>("binding");
        std::cout << "[storage] load result: " << (data.has_value() ? "has_value" : "nullopt") << std::endl;
        if (!data.has_value()) {
            return std::nullopt;
        }
        std::cout << "[storage] raw data: \"" << data.value() << "\"" << std::endl;
        auto result = deserialize_binding(data.value());
        std::cout << "[storage] deserialize ok, vin=\"" << result.vin << "\"" << std::endl;
        return result;
    } catch (const std::exception& e) {
        std::cerr << "[storage] Failed to read vehicle binding: " << e.what() << std::endl;
        return std::nullopt;
    }
}

ErrorCode ProtectedStorageImpl::write_vehicle_binding(const VehicleBinding& binding) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (write_protected_) {
        return ErrorCode::SECURITY_ACCESS_NOT_GRANTED;
    }
    
    try {
        std::string data = serialize_binding(binding);
        if (!store_.save<std::string>("binding", data)) {
            return ErrorCode::STORAGE_ERROR;
        }
        
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Failed to write vehicle binding: " << e.what() << std::endl;
        return ErrorCode::STORAGE_ERROR;
    }
}

std::optional<VehicleConfig> ProtectedStorageImpl::read_vehicle_config() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto data = store_.load<std::string>("config");
        if (!data.has_value()) {
            return std::nullopt;
        }
        return deserialize_config(data.value());
    } catch (const std::exception& e) {
        std::cerr << "Failed to read vehicle config: " << e.what() << std::endl;
        return std::nullopt;
    }
}

ErrorCode ProtectedStorageImpl::write_vehicle_config(const VehicleConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (write_protected_) {
        return ErrorCode::SECURITY_ACCESS_NOT_GRANTED;
    }
    
    try {
        std::string data = serialize_config(config);
        if (!store_.save<std::string>("config", data)) {
            return ErrorCode::STORAGE_ERROR;
        }
        
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Failed to write vehicle config: " << e.what() << std::endl;
        return ErrorCode::STORAGE_ERROR;
    }
}

std::optional<ProductionInfo> ProtectedStorageImpl::read_production_info() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto data = store_.load<std::string>("production_info");
        if (!data.has_value()) {
            return std::nullopt;
        }
        return deserialize_production_info(data.value());
    } catch (const std::exception& e) {
        std::cerr << "Failed to read production info: " << e.what() << std::endl;
        return std::nullopt;
    }
}

ErrorCode ProtectedStorageImpl::write_production_info(const ProductionInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (write_protected_) {
        return ErrorCode::SECURITY_ACCESS_NOT_GRANTED;
    }
    
    try {
        std::string data = serialize_production_info(info);
        if (!store_.save<std::string>("production_info", data)) {
            return ErrorCode::STORAGE_ERROR;
        }
        
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Failed to write production info: " << e.what() << std::endl;
        return ErrorCode::STORAGE_ERROR;
    }
}

ErrorCode ProtectedStorageImpl::set_write_protection(bool locked) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::string data = locked ? "1" : "0";
        if (!store_.save<std::string>("protection", data)) {
            return ErrorCode::STORAGE_ERROR;
        }
        
        write_protected_ = locked;
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Failed to set write protection: " << e.what() << std::endl;
        return ErrorCode::STORAGE_ERROR;
    }
}

bool ProtectedStorageImpl::is_write_protected() {
    std::lock_guard<std::mutex> lock(mutex_);
    return write_protected_;
}

ErrorCode ProtectedStorageImpl::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 删除所有存储的键
        store_.remove("binding");
        store_.remove("config");
        store_.remove("production_info");
        store_.remove("protection");
        
        write_protected_ = false;
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Failed to clear storage: " << e.what() << std::endl;
        return ErrorCode::STORAGE_ERROR;
    }
}





std::string ProtectedStorageImpl::serialize_binding(const VehicleBinding& binding) {
    nlohmann::json j;
    j["vin"] = binding.vin;
    j["ecu_uid"] = binding.ecu_uid;
    j["state"] = static_cast<uint8_t>(binding.state);
    j["locked"] = binding.locked;
    j["bound_at"] = std::chrono::system_clock::to_time_t(binding.bound_at);
    j["last_error"] = binding.last_error;
    j["retry_count"] = binding.retry_count;
    j["rewrite_count"] = binding.rewrite_count;
    j["last_rewrite_at"] = std::chrono::system_clock::to_time_t(binding.last_rewrite_at);
    return j.dump(4);
}

VehicleBinding ProtectedStorageImpl::deserialize_binding(const std::string& data) {
    nlohmann::json j = nlohmann::json::parse(data);
    VehicleBinding binding;
    binding.vin = j["vin"];
    binding.ecu_uid = j["ecu_uid"];
    binding.state = static_cast<ProvisionState>(j["state"]);
    binding.locked = j["locked"];
    binding.bound_at = std::chrono::system_clock::from_time_t(j["bound_at"]);
    binding.last_error = j["last_error"];
    binding.retry_count = j["retry_count"];
    binding.rewrite_count = j["rewrite_count"];
    binding.last_rewrite_at = std::chrono::system_clock::from_time_t(j["last_rewrite_at"]);
    return binding;
}

std::string ProtectedStorageImpl::serialize_config(const VehicleConfig& config) {
    nlohmann::json j;
    j["variant_coding"] = config.variant_coding;
    j["written_at"] = std::chrono::system_clock::to_time_t(config.written_at);
    j["verified"] = config.verified;
    return j.dump(4);
}

VehicleConfig ProtectedStorageImpl::deserialize_config(const std::string& data) {
    nlohmann::json j = nlohmann::json::parse(data);
    VehicleConfig config;
    config.variant_coding = j["variant_coding"].get<std::vector<uint8_t>>();
    config.written_at = std::chrono::system_clock::from_time_t(j["written_at"]);
    config.verified = j["verified"];
    return config;
}

std::string ProtectedStorageImpl::serialize_production_info(const ProductionInfo& info) {
    nlohmann::json j;
    j["production_date"] = info.production_date;
    j["batch_num"] = info.batch_num;
    j["station_id"] = info.station_id;
    j["written_at"] = std::chrono::system_clock::to_time_t(info.written_at);
    return j.dump(4);
}

ProductionInfo ProtectedStorageImpl::deserialize_production_info(const std::string& data) {
    nlohmann::json j = nlohmann::json::parse(data);
    ProductionInfo info;
    info.production_date = j["production_date"];
    info.batch_num = j["batch_num"];
    info.station_id = j["station_id"];
    info.written_at = std::chrono::system_clock::from_time_t(j["written_at"]);
    return info;
}

} // namespace prov
} // namespace tbox