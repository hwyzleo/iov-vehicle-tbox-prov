//
// TBOX-PROV-DSN-CR-008 §14.3: SeUidProvider 实现。
// 读取策略由原 EcuUid::read_uid_detailed() 拆分而来，语义不变：
//   - SE 在位：读 SE UID，失败 -> PROV-1008
//   - SE 缺失 + 测试构建：读 ecu.uid，缺失 -> PROV-1009
//   - SE 缺失 + 生产构建：fail-closed -> PROV-1010
//

#include "se_uid_provider.h"
#include "ecu_uid.h"
#include "error_codes.h"

namespace tbox {
namespace prov {

UidReadResult SeHardwareUidProvider::readUidDetailed() const {
    // 生产环境：SE 优先，无 SE 时 fail-closed（禁止配置文件兜底）
    if (EcuUid::is_se_hardware_present()) {
        std::string uid = EcuUid::read_from_se();
        if (!uid.empty()) {
            return UidReadResult(uid);
        }
        // SE 在位但读取失败/超时
        return UidReadResult(ErrorCode::SE_UID_READ_FAILED, "SE UID读取失败/超时");
    }
    // 生产环境无 SE，禁止配置文件兜底
    return UidReadResult(ErrorCode::SE_MISSING_PRODUCTION_FAIL_CLOSED,
                         "生产环境无SE，禁止配置文件兜底");
}

UidReadResult ConfigSeUidProvider::readUidDetailed() const {
    // 测试构建：SE 优先，SE 缺失时读 ecu.uid 兜底
    if (EcuUid::is_se_hardware_present()) {
        std::string uid = EcuUid::read_from_se();
        if (!uid.empty()) {
            return UidReadResult(uid);
        }
        return UidReadResult(ErrorCode::SE_UID_READ_FAILED, "SE UID读取失败/超时");
    }

    // SE 硬件缺失，从框架配置读取（仅测试环境）
    auto config_uid = EcuUid::read_uid_from_config();
    if (config_uid.has_value()) {
        return UidReadResult(config_uid.value());
    }
    // 配置缺失或无对应 UID
    return UidReadResult(ErrorCode::SE_MISSING_CONFIG_NOT_FOUND,
                         "无SE且配置缺失/无对应UID");
}

std::unique_ptr<SeUidProvider> createSeUidProvider() {
#ifdef TEST_ENVIRONMENT
    return std::make_unique<ConfigSeUidProvider>();
#else
    return std::make_unique<SeHardwareUidProvider>();
#endif
}

} // namespace prov
} // namespace tbox
