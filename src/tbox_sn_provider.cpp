#include "tbox_sn_provider.h"
#include "config.h"
#include <iostream>

namespace tbox {
namespace prov {

namespace {

// 检查是否为测试环境（与 EcuUid::is_test_environment 一致）
bool is_test_environment() {
#ifdef TEST_ENVIRONMENT
    return true;
#else
    return false;
#endif
}

} // anonymous namespace

// ============================================================
// PlatformTboxSnProvider
// ============================================================
SnReadResult PlatformTboxSnProvider::readSn() const {
    // 生产环境：从 TBOX SN 权威来源读取。
    // 具体平台适配（如读取 OTP / efuse / 平台 SN 寄存器）封装在此，
    // 不暴露给 PROV 业务层。
    //
    // 当前桩实现：无可用平台 SN 权威来源，返回 SN 不可用。
    // 生产集成时替换为真实平台读取；失败时返回独立 SN 不可用状态，
    // 不得回退 HSM UID 或 prov.sn。
    return SnReadResult("platform", "production", "read_platform",
                        "平台 SN 权威来源不可用（生产桩实现）");
}

// ============================================================
// ConfigTboxSnProvider
// ============================================================
SnReadResult ConfigTboxSnProvider::readSn() const {
#ifdef TEST_ENVIRONMENT
    auto cfg = CONFIG_SNAPSHOT;
    if (!cfg) {
        return SnReadResult("config", "test", "read_config",
                            "配置快照不可用");
    }

    // 仅读取 prov.sn；严禁读取 prov.uid / ecu.uid（HSM UID 来源隔离）
    if (!cfg->has("prov.sn")) {
        return SnReadResult("config", "test", "missing",
                            "prov.sn 缺失");
    }

    std::string sn = cfg->getString("prov.sn");
    if (sn.empty()) {
        // 空串按 SN 不可用处理，不得复制 ecu_uid
        return SnReadResult("config", "test", "empty",
                            "prov.sn 为空串");
    }

    return SnReadResult(sn, "config", "test");
#else
    // 生产构建不含以 prov.sn 替代生产 SN 权威来源的代码路径
    return SnReadResult("config", "production", "disabled",
                        "生产构建禁用配置来源 SN");
#endif
}

// ============================================================
// 工厂
// ============================================================
std::unique_ptr<TboxSnProvider> createTboxSnProvider() {
    if (is_test_environment()) {
        return std::make_unique<ConfigTboxSnProvider>();
    }
    return std::make_unique<PlatformTboxSnProvider>();
}

} // namespace prov
} // namespace tbox
