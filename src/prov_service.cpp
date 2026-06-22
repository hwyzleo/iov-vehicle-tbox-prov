#include "prov_service.h"
#include "protected_storage_impl.h"
#include <iostream>
#include <chrono>

namespace tbox {
namespace prov {

ProvService::ProvService() {
    config_.storage_path = "/var/tbox/prov";
    config_.enable_write_protection = true;
    config_.max_retry_count = 3;
}

ProvService::ProvService(const ProvServiceConfig& config) 
    : config_(config) {
}

ErrorCode ProvService::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return ErrorCode::SUCCESS;
    }
    
    // 初始化存储
    ErrorCode result = initialize_storage();
    if (result != ErrorCode::SUCCESS) {
        return result;
    }
    
    initialized_ = true;
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::write_vin(const std::string& vin) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 验证VIN格式
    ErrorCode result = validate_vin_format(vin);
    if (result != ErrorCode::SUCCESS) {
        return result;
    }
    
    // 执行VIN写入和绑定流程
    return execute_vin_binding_flow(vin);
}

std::string ProvService::read_vin() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return "";
    }
    
    auto binding = storage_->read_vehicle_binding();
    if (binding.has_value()) {
        return binding->vin;
    }
    
    return "";
}

VehicleBinding ProvService::read_binding() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    VehicleBinding binding;
    if (!initialized_) {
        return binding;
    }
    
    auto stored_binding = storage_->read_vehicle_binding();
    if (stored_binding.has_value()) {
        return stored_binding.value();
    }
    
    return binding;
}

ProvisionState ProvService::get_provision_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ProvisionState::NONE;
    }
    
    auto binding = storage_->read_vehicle_binding();
    if (binding.has_value()) {
        return binding->state;
    }
    
    return ProvisionState::NONE;
}

ErrorCode ProvService::write_vehicle_config(const std::vector<uint8_t>& config_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 检查写保护
    if (is_write_protected()) {
        return ErrorCode::SECURITY_ACCESS_NOT_GRANTED;
    }
    
    // 创建车辆配置
    VehicleConfig config;
    config.variant_coding = config_data;
    config.written_at = std::chrono::system_clock::now();
    config.verified = false;
    
    // 写入存储
    ErrorCode result = storage_->write_vehicle_config(config);
    if (result != ErrorCode::SUCCESS) {
        return ErrorCode::CONFIG_WRITE_FAILED;
    }
    
    // 回读校验
    auto stored_config = storage_->read_vehicle_config();
    if (!stored_config.has_value()) {
        return ErrorCode::CONFIG_WRITE_FAILED;
    }
    
    if (stored_config->variant_coding != config_data) {
        return ErrorCode::READBACK_VERIFICATION_FAILED;
    }
    
    // 更新验证状态
    config.verified = true;
    storage_->write_vehicle_config(config);
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::write_production_info(const ProductionInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 检查写保护
    if (is_write_protected()) {
        return ErrorCode::SECURITY_ACCESS_NOT_GRANTED;
    }
    
    // 设置写入时间
    ProductionInfo new_info = info;
    new_info.written_at = std::chrono::system_clock::now();
    
    // 写入存储
    ErrorCode result = storage_->write_production_info(new_info);
    if (result != ErrorCode::SUCCESS) {
        return ErrorCode::PRODUCTION_INFO_WRITE_FAILED;
    }
    
    // 回读校验
    auto stored_info = storage_->read_production_info();
    if (!stored_info.has_value()) {
        return ErrorCode::PRODUCTION_INFO_WRITE_FAILED;
    }
    
    if (stored_info->production_date != new_info.production_date ||
        stored_info->batch_num != new_info.batch_num ||
        stored_info->station_id != new_info.station_id) {
        return ErrorCode::READBACK_VERIFICATION_FAILED;
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::authorize_rewrite(const std::string& new_vin) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 验证VIN格式
    ErrorCode result = validate_vin_format(new_vin);
    if (result != ErrorCode::SUCCESS) {
        return result;
    }
    
    // 检查当前绑定状态
    auto binding = storage_->read_vehicle_binding();
    if (binding.has_value() && binding->locked) {
        // 已锁定，需要授权重写
        if (new_vin != binding->vin) {
            // VIN冲突，需要授权
            return ErrorCode::VIN_CONFLICT_UNAUTHORIZED;
        }
    }
    
    // 执行重写
    return execute_vin_binding_flow(new_vin);
}

bool ProvService::is_initialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

ErrorCode ProvService::initialize_storage() {
    storage_ = std::make_unique<ProtectedStorageImpl>(config_.storage_path);
    return storage_->initialize();
}

ErrorCode ProvService::execute_vin_binding_flow(const std::string& vin) {
    // 读取ECU UID
    std::string ecu_uid = read_ecu_uid();
    if (ecu_uid.empty()) {
        return ErrorCode::ECU_UID_READ_ERROR;
    }
    
    // 建立绑定
    ErrorCode result = establish_binding(vin, ecu_uid);
    if (result != ErrorCode::SUCCESS) {
        return result;
    }
    
    // 回读校验
    result = verify_write(vin);
    if (result != ErrorCode::SUCCESS) {
        return result;
    }
    
    // 设置写保护
    if (config_.enable_write_protection) {
        result = set_write_protection(true);
        if (result != ErrorCode::SUCCESS) {
            return result;
        }
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::validate_vin_format(const std::string& vin) {
    if (!VinValidator::validate(vin)) {
        return ErrorCode::INVALID_VIN_FORMAT;
    }
    return ErrorCode::SUCCESS;
}

std::string ProvService::read_ecu_uid() {
    return EcuUid::read_uid();
}

ErrorCode ProvService::establish_binding(const std::string& vin, const std::string& ecu_uid) {
    // 检查是否已有绑定
    auto existing_binding = storage_->read_vehicle_binding();
    
    VehicleBinding binding;
    binding.vin = vin;
    binding.ecu_uid = ecu_uid;
    binding.state = ProvisionState::VIN_WRITTEN;
    binding.locked = false;
    binding.bound_at = std::chrono::system_clock::now();
    binding.retry_count = 0;
    binding.rewrite_count = 0;
    
    // 如果是重写，保留重写次数
    if (existing_binding.has_value() && existing_binding->vin == vin) {
        binding.rewrite_count = existing_binding->rewrite_count + 1;
        binding.last_rewrite_at = std::chrono::system_clock::now();
    }
    
    // 写入存储
    ErrorCode result = storage_->write_vehicle_binding(binding);
    if (result != ErrorCode::SUCCESS) {
        return ErrorCode::VIN_WRITE_FAILED;
    }
    
    // 更新状态为已绑定
    binding.state = ProvisionState::BOUND;
    result = storage_->write_vehicle_binding(binding);
    if (result != ErrorCode::SUCCESS) {
        return ErrorCode::VIN_WRITE_FAILED;
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::verify_write(const std::string& expected_vin) {
    auto binding = storage_->read_vehicle_binding();
    if (!binding.has_value()) {
        return ErrorCode::READBACK_VERIFICATION_FAILED;
    }
    
    if (binding->vin != expected_vin) {
        return ErrorCode::READBACK_VERIFICATION_FAILED;
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::set_write_protection(bool locked) {
    return storage_->set_write_protection(locked);
}

bool ProvService::is_write_protected() const {
    return storage_->is_write_protected();
}

void ProvService::record_error(ErrorCode error, const std::string& context) {
    std::cerr << "PROV Error [" << error_code_to_string(error) << "]: " << context << std::endl;
    
    // 更新绑定信息中的错误记录
    auto binding = storage_->read_vehicle_binding();
    if (binding.has_value()) {
        binding->last_error = error_code_to_string(error) + ": " + context;
        binding->retry_count++;
        storage_->write_vehicle_binding(binding.value());
    }
}

} // namespace prov
} // namespace tbox