//
// TBOX-PROV-DSN-CR-008: ProvApplication 实现。
// 组合根装配顺序：Store -> Providers -> Service -> Dispatcher -> ipc::Server
// 清理顺序（§14.4）：beginShutdown -> ipc::Server::stop -> dispatcher_.reset
//                   -> service_.reset -> providers reset -> store reset
//

#include "prov_application.h"
#include "prov_service.h"
#include "prov_ipc_dispatcher.h"
#include "prov_log_adapter.h"
#include "log.h"
#include "config.h"

#include <csignal>
#include <chrono>

namespace tbox {
namespace prov {

ProvApplication::ProvApplication() = default;

// 析构定义于 .cpp：此时 ProvService/ProvIpcDispatcher 已完整可见，
// unique_ptr 析构方可实例化（避免 incomplete type）。
ProvApplication::~ProvApplication() = default;

// ============================================================
// 服务标识与信号
// ============================================================

std::string ProvApplication::getServiceName() const {
    return "prov";
}

std::vector<int> ProvApplication::gracefulSignals() const {
    // CR-008 §6: SIGINT/SIGTERM/SIGHUP 触发 graceful shutdown；
    // SIGHUP 本期不做进程内热重载。
    return {SIGINT, SIGTERM, SIGHUP};
}

std::vector<int> ProvApplication::ignoredSignals() const {
    // SIGPIPE 忽略；socket 错误由 framework-ipc 映射 FW-03xx。
    return {SIGPIPE};
}

// ============================================================
// 初始化
// ============================================================

bool ProvApplication::initialize() {
    // Application::run 已完成 Config::load + Logger::init + 信号安装。
    // 此处只获取配置快照并装配业务组件。
    auto cfg = getConfigSnapshot();
    if (!cfg) {
        ProvLogAdapter::service().error(
            "prov.application.config_unavailable",
            "Config snapshot unavailable after Application::load_config");
        return false;
    }

    // ---- 读取配置（framework-config 类型化访问，不使用 deprecated getConfig）----
    std::string store_root = cfg->getString("common.store.root", "/var/tbox");
    ipc_socket_path_ = cfg->getString("prov.ipc.socket_path",
                                       "/tmp/tbox-prov.sock");
    ipc_config_.max_frame_bytes = static_cast<uint32_t>(
        cfg->getInt("common.ipc.max_frame_bytes", 10485760));
    ipc_config_.receive_timeout_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.receive_timeout_ms", 60000));
    ipc_config_.connect_timeout_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.connect_timeout_ms", 3000));
    ipc_config_.listen_backlog =
        cfg->getInt("common.ipc.listen_backlog", 5);
    ipc_config_.reconnect.initial_backoff_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.reconnect.initial_backoff_ms", 100));
    ipc_config_.reconnect.max_backoff_ms = static_cast<uint32_t>(
        cfg->getInt("common.ipc.reconnect.max_backoff_ms", 5000));
    ipc_config_.reconnect.multiplier =
        cfg->getDouble("common.ipc.reconnect.multiplier", 2.0);

    ProvServiceConfig svc_config;
    svc_config.enable_write_protection =
        cfg->getBool("storage.enable_write_protection", true);
    svc_config.max_retry_count =
        static_cast<uint32_t>(cfg->getInt("storage.max_retry_count", 3));

    // ---- a. Store ----
    store_.emplace("prov", store_root);
    if (!store_->initialize()) {
        ProvLogAdapter::service().error(
            "prov.application.store_init_failed",
            "Store initialize failed",
            {tbox::fw::log::Field("store_root",
                tbox::fw::log::FieldValue::makeString(store_root))});
        store_.reset();
        return false;
    }
    init_stage_ = InitStage::StoreOpened;

    // ---- b. Providers（UID / SN 独立来源）----
    uid_provider_ = createSeUidProvider();
    sn_provider_ = createTboxSnProvider();
    init_stage_ = InitStage::ProvidersReady;

    // ---- c. Service（注入 Store + Providers）----
    service_ = std::make_unique<ProvService>(
        *store_, svc_config, *uid_provider_, *sn_provider_);
    if (service_->initialize() != ErrorCode::SUCCESS) {
        ProvLogAdapter::service().error(
            "prov.application.service_init_failed",
            "ProvService initialize failed");
        rollbackInitialization();
        return false;
    }
    init_stage_ = InitStage::ServiceReady;

    // ---- d. Dispatcher + ipc::Server（传输与适配由组合根持有）----
    dispatcher_ = std::make_unique<ProvIpcDispatcher>(service_.get());
    ipc_server_ = std::make_unique<tbox::fw::ipc::Server>(
        ipc_socket_path_, ipc_config_);

    auto* dispatcher_ptr = dispatcher_.get();
    auto request_handler = [dispatcher_ptr](uint32_t method_id,
                                            std::string_view params_json,
                                            int client_fd) -> std::string {
        return dispatcher_ptr->dispatch(method_id, params_json, client_fd);
    };
    auto disconnect_handler = [](int client_fd) {
        ProvLogAdapter::ipc().debug(
            "prov.ipc.client_disconnected", "Client disconnected",
            {tbox::fw::log::Field("client_fd",
                tbox::fw::log::FieldValue::makeInt(client_fd))});
    };

    if (!ipc_server_->start(std::move(request_handler),
                            std::move(disconnect_handler))) {
        ProvLogAdapter::service().error(
            "prov.application.ipc_start_failed",
            "IPC server start failed",
            {tbox::fw::log::Field("socket_path",
                tbox::fw::log::FieldValue::makeString(ipc_socket_path_))});
        // bind/listen 失败后不得遗留 socket 路径
        ipc_server_.reset();
        dispatcher_.reset();
        rollbackInitialization();
        return false;
    }
    init_stage_ = InitStage::IpcStarted;

    ProvLogAdapter::ipc().info(
        "prov.ipc.started", "IPC server started",
        {tbox::fw::log::Field("socket_path",
            tbox::fw::log::FieldValue::makeString(ipc_socket_path_))});
    return true;
}

void ProvApplication::rollbackInitialization() {
    // 逆序释放已完成的阶段（CR-008 §4.3）
    if (init_stage_ == InitStage::IpcStarted) {
        if (ipc_server_) ipc_server_->stop();
        ipc_server_.reset();
        dispatcher_.reset();
        init_stage_ = InitStage::ServiceReady;
    }
    if (init_stage_ == InitStage::ServiceReady) {
        service_.reset();
        init_stage_ = InitStage::ProvidersReady;
    }
    if (init_stage_ == InitStage::ProvidersReady) {
        sn_provider_.reset();
        uid_provider_.reset();
        init_stage_ = InitStage::StoreOpened;
    }
    if (init_stage_ == InitStage::StoreOpened) {
        // tbox::framework::Store 原子写已逐次落盘，无额外 flush
        store_.reset();
        init_stage_ = InitStage::None;
    }
}

// ============================================================
// 长驻执行
// ============================================================

int ProvApplication::execute() {
    ProvLogAdapter::service().info(
        "prov.service.ready", "PROV service is ready");
    // 不维护私有 running 标志，不依赖 EINTR；统一等待 Application 退出状态。
    waitForShutdown(std::chrono::milliseconds{100});
    return 0;
}

// ============================================================
// 清理（幂等有序停机）
// ============================================================

void ProvApplication::cleanup() {
    // 1. Quiesce：拒绝新业务写入，收敛在途事务边界
    if (service_) {
        service_->beginShutdown();
    }
    // 2. Stop IPC：中断 accept/read，关闭连接与订阅，join 连接线程
    if (ipc_server_) {
        ipc_server_->stop();
    }
    // 3. 销毁 dispatcher（线程退出后才销毁，防 use-after-free）
    dispatcher_.reset();
    // 4. 销毁 service（业务内核及 ProtectedStorage）
    service_.reset();
    // 5. 释放 Providers 与缓存
    sn_provider_.reset();
    uid_provider_.reset();
    // 6. 释放存储（framework-store 原子写已落盘）
    store_.reset();
    init_stage_ = InitStage::None;
    // 最终日志 flush 由 Application::run 在 cleanup 后统一完成。
}

} // namespace prov
} // namespace tbox
