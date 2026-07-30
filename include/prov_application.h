#pragma once

//
// TBOX-PROV-DSN-CR-008: ProvApplication 作为组合根（Composition Root）。
// 继承 hwyz::Application，统一编排配置加载、framework-log 初始化、信号安装、
// 长驻执行与最终 flush；装配并释放 Store / SeUidProvider / TboxSnProvider /
// ProvService / ProvIpcDispatcher / framework-ipc Server。
//
// 生命周期不变量（CR-008 §14.4）：
//   cleanup 顺序 = beginShutdown -> ipc::Server::stop -> dispatcher_.reset
//                  -> service_.reset -> providers reset -> store reset
//   dispatcher_ 必须先于 service_ 销毁（dispatcher 持 service 裸指针）；
//   dispatcher_ 必须在 ipc::Server::stop join 线程后才销毁（防 use-after-free）。
//

#include "application.h"
#include "framework_store.h"
#include "se_uid_provider.h"
#include "tbox_sn_provider.h"
#include "ipc.h"
#include "ipc_types.h"

#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace tbox {
namespace prov {

class ProvService;
class ProvIpcDispatcher;

// 注：未标记 final，以便白盒单元测试通过派生访问 protected 生命周期钩子；
// 生产语义上 ProvApplication 仍为组合根叶子类，不作为扩展基类。
class ProvApplication : public hwyz::Application {
public:
    ProvApplication();
    ~ProvApplication() override;

protected:
    // ============ 服务标识与信号 ============
    std::string getServiceName() const override;
    // CR-008 §6: SIGHUP 本期为 graceful shutdown（不做热重载）
    std::vector<int> gracefulSignals() const override;
    std::vector<int> ignoredSignals() const override;

    // ============ 生命周期 ============
    bool initialize() override;
    int execute() override;
    void cleanup() override;

private:
    enum class InitStage {
        None,
        StoreOpened,
        ProvidersReady,
        ServiceReady,
        IpcStarted
    };

    // 逆序释放已完成的初始化阶段（initialize 失败时调用）
    void rollbackInitialization();

    InitStage init_stage_{InitStage::None};

    // 组合根持有的全部组件（RAII）
    std::optional<tbox::framework::Store> store_;
    std::unique_ptr<SeUidProvider> uid_provider_;
    std::unique_ptr<TboxSnProvider> sn_provider_;
    std::unique_ptr<ProvService> service_;
    std::unique_ptr<ProvIpcDispatcher> dispatcher_;
    std::unique_ptr<tbox::fw::ipc::Server> ipc_server_;

    // initialize 读取、execute/cleanup 复用的配置
    std::string ipc_socket_path_;
    tbox::fw::ipc::IpcConfig ipc_config_{};
};

} // namespace prov
} // namespace tbox
