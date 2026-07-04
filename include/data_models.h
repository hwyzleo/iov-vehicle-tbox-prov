#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <optional>
#include <vector>

namespace tbox {
namespace prov {

enum class ProvisionState : uint8_t {
    NONE = 0,        // 未开始
    VIN_WRITTEN = 1, // VIN 已写待绑
    BOUND = 2,       // 已绑定
    FAILED = 3       // 失败
};

struct VehicleBinding {
    std::string vin;                    // 17位VIN码
    std::string ecu_uid;                // ECU硬件序列号
    ProvisionState state = ProvisionState::NONE;
    bool locked = false;                // 写保护标志
    std::chrono::system_clock::time_point bound_at;  // 绑定时间
    std::string last_error;             // 最后错误信息
    uint32_t retry_count = 0;           // 重试次数
    uint32_t rewrite_count = 0;         // 重写次数
    std::chrono::system_clock::time_point last_rewrite_at;  // 最后重写时间
};

struct VehicleConfig {
    std::vector<uint8_t> variant_coding;  // 配置字/编码blob
    std::chrono::system_clock::time_point written_at;
    bool verified = false;
};

struct ProductionInfo {
    std::string production_date;        // 生产日期
    std::string batch_num;              // 批次号
    std::string station_id;             // 工位ID
    std::chrono::system_clock::time_point written_at;
};

// VIN格式约束
struct VinConstraints {
    static constexpr size_t VIN_LENGTH = 17;
    static constexpr char EXCLUDED_CHARS[] = "IOQ";
    static constexpr size_t CHECK_DIGIT_POSITION = 9;  // 第9位是校验位
};

} // namespace prov
} // namespace tbox