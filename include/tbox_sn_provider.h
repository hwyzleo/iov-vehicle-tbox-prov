#pragma once

#include <string>
#include <memory>
#include "error_codes.h"

namespace tbox {
namespace prov {

/// SN 读取结果（与 UidReadResult 对称，但独立）
/// sn 表示 TBOX 设备序列号；ecu_uid 表示 HSM 唯一 ID。
/// 两者独立读取、独立校验、独立存储，不共享槽位且不得互相兜底。
struct SnReadResult {
    std::string sn;
    bool success;
    std::string error_message;   // 失败原因（不含原始配置值）
    std::string sn_source;       // 来源标识：platform / config / none
    std::string environment;     // production / test
    std::string failure_stage;   // read_platform / read_config / missing / empty / disabled

    SnReadResult() : success(false) {}
    SnReadResult(const std::string& sn, const std::string& source, const std::string& env)
        : sn(sn), success(true), sn_source(source), environment(env) {}
    SnReadResult(const std::string& source, const std::string& env,
                 const std::string& stage, const std::string& error)
        : success(false), error_message(error), sn_source(source),
          environment(env), failure_stage(stage) {}
};

/// TBOX SN 来源抽象
///
/// 封装 TBOX 设备序列号的读取，PROV 业务层不直接依赖平台读取细节。
/// SN 获取失败时返回独立的 SN 不可用状态；不得回退 HSM UID（ecu_uid）。
class TboxSnProvider {
public:
    virtual ~TboxSnProvider() = default;

    /// 读取 TBOX SN
    /// @return 成功返回 sn；失败返回 SN 不可用状态（success=false）
    virtual SnReadResult readSn() const = 0;
};

/// 生产环境 Provider：从 TBOX SN 权威来源读取
/// 具体平台适配封装在 Provider 内，不暴露给 PROV 业务层。
class PlatformTboxSnProvider : public TboxSnProvider {
public:
    SnReadResult readSn() const override;
};

/// 测试构建 Provider：从服务本地 ./prov.yaml 的独立可选项 prov.sn 读取
/// 仅测试构建使用；不得读取 prov.uid / ecu.uid；空串按 SN 不可用处理。
class ConfigTboxSnProvider : public TboxSnProvider {
public:
    SnReadResult readSn() const override;
};

/// 默认 Provider 选择工厂
/// 生产构建 -> PlatformTboxSnProvider（TBOX SN 权威来源）
/// 测试构建 -> ConfigTboxSnProvider（读取 prov.sn）
std::unique_ptr<TboxSnProvider> createTboxSnProvider();

} // namespace prov
} // namespace tbox
