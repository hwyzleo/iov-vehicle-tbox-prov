#include "prov_service.h"
#include "prov_log_adapter.h"
#include "prov_context.h"
#include "protected_storage_impl.h"
#include "log.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace tbox {
namespace prov {

namespace {
// 脱敏辅助函数
std::string hash_identifier(const std::string& type, const std::string& value) {
    // 使用简单的哈希实现（实际项目中应使用 SHA-256）
    // 这里使用 framework 的 Identifier 敏感度标记
    std::hash<std::string> hasher;
    auto h = hasher(value);
    std::ostringstream oss;
    oss << type << "-" << std::hex << std::setfill('0') << std::setw(16) << h;
    return oss.str();
}

std::string hash_vin(const std::string& vin) {
    return hash_identifier("vin", vin);
}

std::string hash_ecu_uid(const std::string& ecu_uid) {
    return hash_identifier("uid", ecu_uid);
}

std::string hash_sn(const std::string& sn) {
    // sn 与 ecu_uid 分别记录为 sn_hash / ecu_uid_hash，不得合并为通用 device_id
    return hash_identifier("sn", sn);
}

std::string hash_config(const std::vector<uint8_t>& config) {
    return hash_identifier("cfg", std::string(config.begin(), config.end()));
}

std::string hash_batch(const std::string& batch) {
    return hash_identifier("batch", batch);
}

std::string hash_station(const std::string& station) {
    return hash_identifier("station", station);
}

std::string provision_state_to_string(ProvisionState state) {
    switch (state) {
        case ProvisionState::NONE: return "NONE";
        case ProvisionState::VIN_WRITTEN: return "VIN_WRITTEN";
        case ProvisionState::BOUND: return "BOUND";
        case ProvisionState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}
} // anonymous namespace

ProvService::ProvService(tbox::framework::Store& store,
                         const ProvServiceConfig& config,
                         SeUidProvider& uid_provider,
                         TboxSnProvider& sn_provider)
    : store_(store), config_(config),
      uid_provider_(uid_provider), sn_provider_(sn_provider) {
}

ProvService::~ProvService() {
    // IPC 传输与 dispatcher 已由 ProvApplication 在 cleanup 中有序停止并销毁；
    // 析构仅确保业务内核资源（ProtectedStorage）释放。
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

void ProvService::beginShutdown() {
    shutting_down_.store(true, std::memory_order_relaxed);
    ProvLogAdapter::service().info("prov.service.shutdown_begin",
        "PROV service entering shutdown, rejecting new writes");
}

ErrorCode ProvService::write_vin(const std::string& vin) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::INVALID_STATE;
    }

    // 停机后拒绝新业务写入（quiesce，CR-008 §14.2）
    if (shutting_down_.load(std::memory_order_relaxed)) {
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
        binding = stored_binding.value();
    }
    
    // 若存储的 sn 为空（含旧快照），经 SN Provider 补齐到返回值（不写回存储）
    // sn 与 ecu_uid 独立：SN 失败不得回退 ecu_uid，反之亦然
    // 仅当存在绑定时补齐 sn（无绑定时不读取 SN）
    if (stored_binding.has_value() && binding.sn.empty()) {
        auto sn_start = std::chrono::steady_clock::now();
        auto sn_result = sn_provider_.readSn();
        auto sn_end = std::chrono::steady_clock::now();
        auto sn_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            sn_end - sn_start).count();
        
        if (sn_result.success) {
            binding.sn = sn_result.sn;
        } else {
            // SN 不可用是异常：ERROR 上报；绑定本身仍有效（readBinding status=0）
            // 不得复制 ecu_uid 作为 sn
            ProvLogAdapter::binding().error("prov.sn.read_failed",
                "Failed to read TBOX SN",
                {tbox::fw::log::Field("sn_source", tbox::fw::log::FieldValue::makeString(sn_result.sn_source)),
                 tbox::fw::log::Field("environment", tbox::fw::log::FieldValue::makeString(sn_result.environment)),
                 tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString(sn_result.failure_stage)),
                 tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(sn_duration_ms)),
                 tbox::fw::log::Field("error_code", tbox::fw::log::FieldValue::makeString(error_code_to_string(ErrorCode::SN_UNAVAILABLE)))}
            );
            // sn 保持空，不回退 ecu_uid
        }
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
    auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 停机后拒绝新业务写入（quiesce，CR-008 §14.2）
    if (shutting_down_.load(std::memory_order_relaxed)) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 检查写保护
    if (is_write_protected()) {
        return ErrorCode::SECURITY_ACCESS_NOT_GRANTED;
    }
    
    auto config_hash = hash_config(config_data);
    
    // 创建车辆配置
    VehicleConfig config;
    config.variant_coding = config_data;
    config.written_at = std::chrono::system_clock::now();
    config.verified = false;
    
    // 写入存储
    ErrorCode result = storage_->write_vehicle_config(config);
    if (result != ErrorCode::SUCCESS) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::vehicle_config().error("prov.vehicle_config.write_failed",
            "Vehicle config write failed",
            {tbox::fw::log::Field("config_hash", tbox::fw::log::FieldValue::makeString(config_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("write_storage")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return ErrorCode::CONFIG_WRITE_FAILED;
    }
    
    // 回读校验
    auto stored_config = storage_->read_vehicle_config();
    if (!stored_config.has_value()) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::vehicle_config().error("prov.vehicle_config.write_failed",
            "Vehicle config readback failed",
            {tbox::fw::log::Field("config_hash", tbox::fw::log::FieldValue::makeString(config_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("readback_empty")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return ErrorCode::CONFIG_WRITE_FAILED;
    }
    
    if (stored_config->variant_coding != config_data) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::vehicle_config().error("prov.vehicle_config.write_failed",
            "Vehicle config readback mismatch",
            {tbox::fw::log::Field("config_hash", tbox::fw::log::FieldValue::makeString(config_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("readback_mismatch")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return ErrorCode::READBACK_VERIFICATION_FAILED;
    }
    
    // 更新验证状态
    config.verified = true;
    storage_->write_vehicle_config(config);
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::write_production_info(const ProductionInfo& info) {
    auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 停机后拒绝新业务写入（quiesce，CR-008 §14.2）
    if (shutting_down_.load(std::memory_order_relaxed)) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 检查写保护
    if (is_write_protected()) {
        return ErrorCode::SECURITY_ACCESS_NOT_GRANTED;
    }
    
    auto batch_hash = hash_batch(info.batch_num);
    auto station_hash = hash_station(info.station_id);
    
    // 设置写入时间
    ProductionInfo new_info = info;
    new_info.written_at = std::chrono::system_clock::now();
    
    // 写入存储
    ErrorCode result = storage_->write_production_info(new_info);
    if (result != ErrorCode::SUCCESS) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::production().error("prov.production_info.write_failed",
            "Production info write failed",
            {tbox::fw::log::Field("batch_hash", tbox::fw::log::FieldValue::makeString(batch_hash)),
             tbox::fw::log::Field("station_hash", tbox::fw::log::FieldValue::makeString(station_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("write_storage")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return ErrorCode::PRODUCTION_INFO_WRITE_FAILED;
    }
    
    // 回读校验
    auto stored_info = storage_->read_production_info();
    if (!stored_info.has_value()) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::production().error("prov.production_info.write_failed",
            "Production info readback failed",
            {tbox::fw::log::Field("batch_hash", tbox::fw::log::FieldValue::makeString(batch_hash)),
             tbox::fw::log::Field("station_hash", tbox::fw::log::FieldValue::makeString(station_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("readback_empty")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return ErrorCode::PRODUCTION_INFO_WRITE_FAILED;
    }
    
    if (stored_info->production_date != new_info.production_date ||
        stored_info->batch_num != new_info.batch_num ||
        stored_info->station_id != new_info.station_id) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::production().error("prov.production_info.write_failed",
            "Production info readback mismatch",
            {tbox::fw::log::Field("batch_hash", tbox::fw::log::FieldValue::makeString(batch_hash)),
             tbox::fw::log::Field("station_hash", tbox::fw::log::FieldValue::makeString(station_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("readback_mismatch")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return ErrorCode::READBACK_VERIFICATION_FAILED;
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::authorize_rewrite(const std::string& new_vin) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::INVALID_STATE;
    }
    
    // 停机后拒绝新业务写入（quiesce，CR-008 §14.2）
    if (shutting_down_.load(std::memory_order_relaxed)) {
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
        auto new_vin_hash = hash_vin(new_vin);
        auto current_vin_hash = hash_vin(binding->vin);
        
        if (new_vin != binding->vin) {
            ProvLogAdapter::rewrite().warn("prov.binding.rewrite_denied",
                "Unauthorized rewrite attempt",
                {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString(new_vin_hash)),
                 tbox::fw::log::Field("current_vin_hash", tbox::fw::log::FieldValue::makeString(current_vin_hash)),
                 tbox::fw::log::Field("reason", tbox::fw::log::FieldValue::makeString("vin_conflict")),
                 tbox::fw::log::Field("rewrite_count", tbox::fw::log::FieldValue::makeInt(binding->rewrite_count))}
            );
            
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
    storage_ = std::make_unique<ProtectedStorageImpl>(store_);
    return storage_->initialize();
}

ErrorCode ProvService::execute_vin_binding_flow(const std::string& vin) {
    auto start = std::chrono::steady_clock::now();
    auto vin_hash = hash_vin(vin);
    
    // 读取ECU UID（经注入的 SeUidProvider，支撑故障注入）
    auto uid_result = uid_provider_.readUidDetailed();
    if (!uid_result.success) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::vin().error("prov.uid.read_failed",
            "Failed to read ECU UID",
            {tbox::fw::log::Field("error_message", tbox::fw::log::FieldValue::makeString(uid_result.error_message)),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return uid_result.error_code;
    }
    
    std::string ecu_uid = uid_result.uid;
    auto ecu_uid_hash = hash_ecu_uid(ecu_uid);
    
    // 建立绑定
    ErrorCode result = establish_binding(vin, ecu_uid);
    if (result != ErrorCode::SUCCESS) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::vin().error("prov.vin.write.failed",
            "VIN write failed",
            {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString(vin_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("establish_binding")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return result;
    }
    
    // 回读校验
    result = verify_write(vin);
    if (result != ErrorCode::SUCCESS) {
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        ProvLogAdapter::binding().error("prov.binding.readback_failed",
            "Binding readback verification failed",
            {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString(vin_hash)),
             tbox::fw::log::Field("ecu_uid_hash", tbox::fw::log::FieldValue::makeString(ecu_uid_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("verify_write")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
        );
        
        return result;
    }
    
    // 设置写保护
    if (config_.enable_write_protection) {
        result = set_write_protection(true);
        if (result != ErrorCode::SUCCESS) {
            return result;
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    ProvLogAdapter::vin().info("prov.vin.write.succeeded",
        "VIN written successfully",
        {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString(vin_hash)),
         tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(duration_ms))}
    );
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::validate_vin_format(const std::string& vin) {
    if (!VinValidator::validate(vin)) {
        auto vin_hash = hash_vin(vin);
        
        ProvLogAdapter::vin().warn("prov.vin.validation_failed",
            "VIN format validation failed",
            {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString(vin_hash)),
             tbox::fw::log::Field("validation_reason", tbox::fw::log::FieldValue::makeString("invalid_format"))}
        );
        
        return ErrorCode::INVALID_VIN_FORMAT;
    }
    return ErrorCode::SUCCESS;
}

std::string ProvService::read_ecu_uid() {
    auto result = uid_provider_.readUidDetailed();
    return result.success ? result.uid : "";
}

ErrorCode ProvService::establish_binding(const std::string& vin, const std::string& ecu_uid) {
    auto vin_hash = hash_vin(vin);
    auto ecu_uid_hash = hash_ecu_uid(ecu_uid);
    
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
    
    auto binding_state = provision_state_to_string(binding.state);
    
    ProvLogAdapter::binding().info("prov.binding.created",
        "VIN-ECU binding established",
        {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString(vin_hash)),
         tbox::fw::log::Field("ecu_uid_hash", tbox::fw::log::FieldValue::makeString(ecu_uid_hash)),
         tbox::fw::log::Field("binding_state", tbox::fw::log::FieldValue::makeString(binding_state)),
         tbox::fw::log::Field("rewrite_count", tbox::fw::log::FieldValue::makeInt(binding.rewrite_count))}
    );
    
    return ErrorCode::SUCCESS;
}

ErrorCode ProvService::verify_write(const std::string& expected_vin) {
    auto binding = storage_->read_vehicle_binding();
    if (!binding.has_value()) {
        return ErrorCode::READBACK_VERIFICATION_FAILED;
    }
    
    if (binding->vin != expected_vin) {
        auto vin_hash = hash_vin(expected_vin);
        auto ecu_uid_hash = hash_ecu_uid(binding->ecu_uid);
        
        ProvLogAdapter::binding().error("prov.binding.readback_failed",
            "Binding readback verification failed",
            {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString(vin_hash)),
             tbox::fw::log::Field("ecu_uid_hash", tbox::fw::log::FieldValue::makeString(ecu_uid_hash)),
             tbox::fw::log::Field("failure_stage", tbox::fw::log::FieldValue::makeString("vin_mismatch"))}
        );
        
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
