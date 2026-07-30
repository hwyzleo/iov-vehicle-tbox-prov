#pragma once

//
// TBOX-PROV-DSN-CR-008 §14.3: ECU/HSM UID 来源抽象为可注入实例。
// 原 EcuUid 静态类保留为对默认实例的转发，供既有单元测试平滑迁移。
//

#include "ecu_uid.h"  // UidReadResult
#include <memory>

namespace tbox {
namespace prov {

/// ECU/HSM UID 来源抽象
///
/// 封装 ECU UID（HSM/SE UID）的读取，PROV 业务层不直接依赖平台读取细节。
/// 由 ProvApplication 创建并注入 ProvService，支撑各阶段故障注入测试。
/// 语义对齐 DSN-CR-003：SE 优先 / ecu.uid 兜底（仅测试）/ 生产 fail-closed。
class SeUidProvider {
public:
    virtual ~SeUidProvider() = default;

    /// 读取 ECU UID（详细结果）
    /// @return 成功返回 uid；失败返回对应 PROV-1008/1009/1010 错误
    virtual UidReadResult readUidDetailed() const = 0;
};

/// 生产环境 Provider：SE 优先，SE 缺失时 fail-closed（PROV-1010）
/// 不读取配置文件兜底，确保生产强物理绑定。
class SeHardwareUidProvider : public SeUidProvider {
public:
    UidReadResult readUidDetailed() const override;
};

/// 测试构建 Provider：SE 优先，SE 缺失时读 ./prov.yaml:ecu.uid（PROV-1009 缺失时报错）
/// 仅测试构建使用；不得读取 prov.sn（与 TboxSnProvider 严格隔离）。
class ConfigSeUidProvider : public SeUidProvider {
public:
    UidReadResult readUidDetailed() const override;
};

/// 默认 Provider 选择工厂
/// 生产构建 -> SeHardwareUidProvider（SE 优先 / fail-closed）
/// 测试构建 -> ConfigSeUidProvider（SE 优先 / ecu.uid 兜底）
std::unique_ptr<SeUidProvider> createSeUidProvider();

} // namespace prov
} // namespace tbox
