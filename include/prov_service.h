#pragma once

//
// TBOX-PROV-DSN-CR-008 §14.2: ProvService 瘦身为纯业务内核。
// - 移除 ipc::Server / ProvIpcDispatcher 所有权（上提至 ProvApplication 组合根）
// - Store / SeUidProvider / TboxSnProvider 经构造注入，不再自建依赖
// - 新增 beginShutdown() 供 ProvApplication 在 cleanup 首步收敛在途事务
// 对外业务接口（VIN/绑定/配置/生产信息）与数据模型不变。
//

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include "tbox/prov/types.h"
#include "tbox/prov/errors.h"
#include "protected_storage.h"
#include "framework_store.h"
#include "vin_validator.h"
#include "se_uid_provider.h"
#include "tbox_sn_provider.h"

namespace tbox {
namespace prov {

/// PROV 业务服务配置（仅业务参数；IPC 传输配置由 ProvApplication 直接读取）
struct ProvServiceConfig {
    bool enable_write_protection = true;
    uint32_t max_retry_count = 3;
};

class ProvService {
public:
    /// @param store        持久化存储（由 ProvApplication 持有，服务持引用）
    /// @param config       业务配置
    /// @param uid_provider ECU/HSM UID 来源（注入，支撑故障注入测试）
    /// @param sn_provider  TBOX SN 来源（注入，与 UID 严格隔离）
    ProvService(tbox::framework::Store& store,
                const ProvServiceConfig& config,
                SeUidProvider& uid_provider,
                TboxSnProvider& sn_provider);
    virtual ~ProvService();

    // 初始化服务
    virtual ErrorCode initialize();

    // TBOX-PROV-DSN-CR-008 §14.2: 进入停机，拒绝新业务写入（quiesce）。
    // 由 ProvApplication::cleanup() 首步调用；幂等。
    virtual void beginShutdown();

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
    tbox::framework::Store& store_;
    ProvServiceConfig config_;
    bool initialized_ = false;

    std::unique_ptr<ProtectedStorage> storage_;
    // UID/SN 来源由 ProvApplication 注入（引用），服务不持有其所有权
    SeUidProvider& uid_provider_;
    TboxSnProvider& sn_provider_;

    // 停机标志：beginShutdown 后拒绝新业务写入
    std::atomic<bool> shutting_down_{false};

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
