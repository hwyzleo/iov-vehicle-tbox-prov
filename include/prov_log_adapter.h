#pragma once

#include "log.h"
#include "log_types.h"
#include <string>

namespace tbox::prov {

/**
 * TBOX-PROV 日志适配器
 * 封装 framework-log 的初始化和模块 Logger 获取
 * 
 * 模块划分：
 * - service:        服务生命周期日志
 * - vin:            VIN 校验/写入日志
 * - binding:        绑定建立/回读日志
 * - rewrite:        授权重写日志
 * - vehicle_config: 车辆配置写入日志
 * - production:     生产信息写入日志
 * - ipc:            IPC 通信日志
 */
class ProvLogAdapter {
public:
    // 初始化日志系统（在 main.cpp 中调用一次）
    static tbox::fw::log::InitResult init(
        const std::string& service,
        const tbox::fw::log::LogConfig& config
    );

    // 获取各模块的 Logger 实例
    static tbox::fw::log::Logger service();
    static tbox::fw::log::Logger vin();
    static tbox::fw::log::Logger binding();
    static tbox::fw::log::Logger rewrite();
    static tbox::fw::log::Logger vehicle_config();
    static tbox::fw::log::Logger production();
    static tbox::fw::log::Logger ipc();

private:
    static bool s_initialized;
};

} // namespace tbox::prov
