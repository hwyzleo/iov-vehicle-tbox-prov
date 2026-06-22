#include "protected_storage_impl.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

namespace tbox {
namespace prov {

ProtectedStorageImpl::ProtectedStorageImpl(const std::string& storage_path) 
    : storage_path_(storage_path) {
}

ErrorCode ProtectedStorageImpl::initialize() {
    try {
        if (!create_directory(storage_path_)) {
            return ErrorCode::STORAGE_ERROR;
        }
        
        // 检查写保护状态
        std::string protection_path = get_protection_file_path();
        if (file_exists(protection_path)) {
            std::string data = read_from_file(protection_path);
            write_protected_ = (data == "1");
        }
        
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Storage initialization failed: " << e.what() << std::endl;
        return ErrorCode::STORAGE_ERROR;
    }
}

std::optional<VehicleBinding> ProtectedStorageImpl::read_vehicle_binding() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string path = get_binding_file_path();
    if (!file_exists(path)) {
        return std::nullopt;
    }
    
    try {
        std::string data = read_from_file(path);
        return deserialize_binding(data);
    } catch (const std::exception& e) {
        std::cerr << "Failed to read vehicle binding: " << e.what() << std::endl;
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
        std::string path = get_binding_file_path();
        
        if (!write_to_file(path, data)) {
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
    
    std::string path = get_config_file_path();
    if (!file_exists(path)) {
        return std::nullopt;
    }
    
    try {
        std::string data = read_from_file(path);
        return deserialize_config(data);
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
        std::string path = get_config_file_path();
        
        if (!write_to_file(path, data)) {
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
    
    std::string path = get_production_info_file_path();
    if (!file_exists(path)) {
        return std::nullopt;
    }
    
    try {
        std::string data = read_from_file(path);
        return deserialize_production_info(data);
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
        std::string path = get_production_info_file_path();
        
        if (!write_to_file(path, data)) {
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
        std::string path = get_protection_file_path();
        std::string data = locked ? "1" : "0";
        
        if (!write_to_file(path, data)) {
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
        std::filesystem::remove_all(storage_path_);
        if (!create_directory(storage_path_)) {
            return ErrorCode::STORAGE_ERROR;
        }
        
        write_protected_ = false;
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Failed to clear storage: " << e.what() << std::endl;
        return ErrorCode::STORAGE_ERROR;
    }
}

std::string ProtectedStorageImpl::get_binding_file_path() const {
    return storage_path_ + "/binding.json";
}

std::string ProtectedStorageImpl::get_config_file_path() const {
    return storage_path_ + "/config.json";
}

std::string ProtectedStorageImpl::get_production_info_file_path() const {
    return storage_path_ + "/production_info.json";
}

std::string ProtectedStorageImpl::get_protection_file_path() const {
    return storage_path_ + "/protection.txt";
}

bool ProtectedStorageImpl::write_to_file(const std::string& path, const std::string& data) {
    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file << data;
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to write to file: " << e.what() << std::endl;
        return false;
    }
}

std::string ProtectedStorageImpl::read_from_file(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (const std::exception& e) {
        std::cerr << "Failed to read from file: " << e.what() << std::endl;
        return "";
    }
}

bool ProtectedStorageImpl::file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool ProtectedStorageImpl::create_directory(const std::string& path) {
    try {
        if (!std::filesystem::exists(path)) {
            return std::filesystem::create_directories(path);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
        return false;
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