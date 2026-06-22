#pragma once

#include <string>
#include <memory>
#include <mutex>
#include "data_models.h"
#include "error_codes.h"
#include "protected_storage.h"
#include "vin_validator.h"
#include "ecu_uid.h"

namespace tbox {
namespace prov {

struct ProvServiceConfig {
    std::string storage_path;
    bool enable_write_protection = true;
    uint32_t max_retry_count = 3;
};

class ProvService {
public:
    ProvService();
    explicit ProvService(const ProvServiceConfig& config);
    virtual ~ProvService() = default;

    // 初始化服务
    virtual ErrorCode initialize();
    
    // 业务接口：写入 VIN
    virtual ErrorCode write_vin(const std::string& vin);
    
    // 业务接口：读取 VIN
    virtual std::string read_vin() const;
    
    // 业务接口：读取绑定信息
    virtual VehicleBinding read_binding() const;
    
    // 业务接口：获取个性化状态
    virtual ProvisionState get_provision_state() const;
    
    // 业务接口：写入车辆配置
    virtual ErrorCode write_vehicle_config(const std::vector<uint8_t>& config_data);
    
    // 业务接口：写入生产信息
    virtual ErrorCode write_production_info(const ProductionInfo& info);
    
    // 业务接口：授权重写
    virtual ErrorCode authorize_rewrite(const std::string& new_vin);
    
    // 检查是否已初始化
    virtual bool is_initialized() const;

protected:
    ProvServiceConfig config_;
    bool initialized_ = false;
    
    std::unique_ptr<ProtectedStorage> storage_;
    
    mutable std::mutex mutex_;
    
    // 初始化存储
    virtual ErrorCode initialize_storage();
    
    // 执行VIN写入和绑定流程
    virtual ErrorCode execute_vin_binding_flow(const std::string& vin);
    
    // 验证VIN格式
    virtual ErrorCode validate_vin_format(const std::string& vin);
    
    // 读取ECU UID
    virtual std::string read_ecu_uid();
    
    // 建立绑定
    virtual ErrorCode establish_binding(const std::string& vin, const std::string& ecu_uid);
    
    // 回读校验
    virtual ErrorCode verify_write(const std::string& expected_vin);
    
    // 设置写保护
    virtual ErrorCode set_write_protection(bool locked);
    
    // 检查写保护状态
    virtual bool is_write_protected() const;
    
    // 记录错误
    virtual void record_error(ErrorCode error, const std::string& context);
};

} // namespace prov
} // namespace tbox
