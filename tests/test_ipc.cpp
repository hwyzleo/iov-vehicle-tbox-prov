#include <gtest/gtest.h>
#include "prov_service.h"
#include "prov_ipc_dispatcher.h"
#include "prov_client.h"
#include "framework_store.h"
#include "ipc.h"
#include "config.h"
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>

using namespace tbox::prov;

// TBOX-PROV-DSN-CR-008: ProvService 不再持有 IPC，测试手动装配 dispatcher + ipc::Server，
// 等价 ProvApplication 的 IPC 部分。
class IpcTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/prov_ipc_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        store_ = std::make_unique<tbox::framework::Store>("prov", test_dir_);
        config_.enable_write_protection = true;
        config_.max_retry_count = 3;
        socket_path_ = test_dir_ + "/test.sock";

        uid_provider_ = createSeUidProvider();
        sn_provider_ = createTboxSnProvider();
        service_ = std::make_unique<ProvService>(*store_, config_, *uid_provider_, *sn_provider_);
    }
    
    void TearDown() override {
        stopIpc();
        service_.reset();
        sn_provider_.reset();
        uid_provider_.reset();
        store_.reset();
        std::filesystem::remove_all(test_dir_);
    }

    /// 手动装配 dispatcher + framework-ipc Server（等价 ProvApplication::initialize 的 IPC 部分）
    bool startIpc() {
        dispatcher_ = std::make_unique<ProvIpcDispatcher>(service_.get());
        ipc_server_ = std::make_unique<tbox::fw::ipc::Server>(socket_path_, tbox::fw::ipc::IpcConfig{});
        auto* disp = dispatcher_.get();
        auto handler = [disp](uint32_t method_id, std::string_view params, int fd) -> std::string {
            return disp->dispatch(method_id, params, fd);
        };
        if (!ipc_server_->start(std::move(handler), {})) {
            ipc_server_.reset();
            dispatcher_.reset();
            return false;
        }
        return true;
    }

    void stopIpc() {
        if (ipc_server_) {
            ipc_server_->stop();
            ipc_server_.reset();
        }
        dispatcher_.reset();
    }
    
    std::string test_dir_;
    std::string socket_path_;
    std::unique_ptr<tbox::framework::Store> store_;
    ProvServiceConfig config_;
    std::unique_ptr<SeUidProvider> uid_provider_;
    std::unique_ptr<TboxSnProvider> sn_provider_;
    std::unique_ptr<ProvService> service_;
    std::unique_ptr<ProvIpcDispatcher> dispatcher_;
    std::unique_ptr<tbox::fw::ipc::Server> ipc_server_;
};

TEST_F(IpcTest, InitializeAndStartIpcServer) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = startIpc();
    EXPECT_TRUE(started);
}

TEST_F(IpcTest, ConnectClient) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = startIpc();
    EXPECT_TRUE(started);
    
    ProvClient client(socket_path_);
    bool connected = client.connect();
    EXPECT_TRUE(connected);
    
    client.disconnect();
}

TEST_F(IpcTest, ReadVinViaIpc) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = startIpc();
    EXPECT_TRUE(started);
    
    ProvClient client(socket_path_);
    bool connected = client.connect();
    EXPECT_TRUE(connected);
    
    // 读取 VIN（初始应该为空）
    std::string read_vin = client.read_vin();
    EXPECT_TRUE(read_vin.empty());
    
    client.disconnect();
}

TEST_F(IpcTest, GetProvisionStateViaIpc) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = startIpc();
    EXPECT_TRUE(started);
    
    ProvClient client(socket_path_);
    bool connected = client.connect();
    EXPECT_TRUE(connected);
    
    // 获取初始状态
    ProvisionState state = client.get_provision_state();
    EXPECT_EQ(state, ProvisionState::NONE);
    
    client.disconnect();
}

TEST_F(IpcTest, MultipleClients) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = startIpc();
    EXPECT_TRUE(started);
    
    // 创建多个客户端
    ProvClient client1(socket_path_);
    ProvClient client2(socket_path_);
    
    bool connected1 = client1.connect();
    bool connected2 = client2.connect();
    EXPECT_TRUE(connected1);
    EXPECT_TRUE(connected2);
    
    // 两个客户端都可以读取
    std::string vin1 = client1.read_vin();
    std::string vin2 = client2.read_vin();
    EXPECT_EQ(vin1, vin2);  // 都应该为空
    
    client1.disconnect();
    client2.disconnect();
}

TEST_F(IpcTest, ClientReconnect) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = startIpc();
    EXPECT_TRUE(started);
    
    ProvClient client(socket_path_);
    
    // 第一次连接
    bool connected = client.connect();
    EXPECT_TRUE(connected);
    
    std::string vin1 = client.read_vin();
    client.disconnect();
    
    // 重新连接
    connected = client.connect();
    EXPECT_TRUE(connected);
    
    std::string vin2 = client.read_vin();
    EXPECT_EQ(vin1, vin2);  // 都应该为空
    
    client.disconnect();
}

TEST_F(IpcTest, ServerShutdown) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = startIpc();
    EXPECT_TRUE(started);
    
    // 停止服务器
    stopIpc();
    
    // 客户端应该无法连接
    ProvClient client(socket_path_);
    bool connected = client.connect();
    EXPECT_FALSE(connected);
}

TEST_F(IpcTest, WriteVinWithoutSecurityAccess) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = startIpc();
    EXPECT_TRUE(started);
    
    ProvClient client(socket_path_);
    bool connected = client.connect();
    EXPECT_TRUE(connected);
    
    client.disconnect();
}

// ============================================================
// framework-ipc 集成测试
// ============================================================

TEST_F(IpcTest, InitializeViaIpcReturnsSuccess) {
    service_->initialize();
    startIpc();
    
    ProvClient client(socket_path_);
    ASSERT_TRUE(client.connect());
    
    ErrorCode result = client.initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    client.disconnect();
}

TEST_F(IpcTest, ReadBindingViaIpcReturnsEmptyBinding) {
    service_->initialize();
    startIpc();
    
    ProvClient client(socket_path_);
    ASSERT_TRUE(client.connect());
    
    VehicleBinding binding = client.read_binding();
    EXPECT_TRUE(binding.vin.empty());
    EXPECT_TRUE(binding.ecu_uid.empty());
    EXPECT_TRUE(binding.sn.empty());  // CR-007: 无绑定时 sn 为空
    EXPECT_EQ(binding.state, ProvisionState::NONE);
    EXPECT_FALSE(binding.locked);
    
    client.disconnect();
}

TEST_F(IpcTest, WriteVinReturnsBusinessError) {
    service_->initialize();
    startIpc();
    
    ProvClient client(socket_path_);
    ASSERT_TRUE(client.connect());
    
    // 尝试写入非法 VIN，应返回业务错误码（非 SUCCESS）
    ErrorCode result = client.write_vin("invalid");
    EXPECT_NE(result, ErrorCode::SUCCESS);
    
    client.disconnect();
}

TEST_F(IpcTest, AuthorizeRewriteReturnsBusinessError) {
    service_->initialize();
    startIpc();
    
    ProvClient client(socket_path_);
    ASSERT_TRUE(client.connect());
    
    // 尝试授权重写非法 VIN，应返回业务错误码
    ErrorCode result = client.authorize_rewrite("bad_vin");
    EXPECT_NE(result, ErrorCode::SUCCESS);
    
    client.disconnect();
}

TEST_F(IpcTest, MultipleSequentialCallsOnSameConnection) {
    service_->initialize();
    startIpc();
    
    ProvClient client(socket_path_);
    ASSERT_TRUE(client.connect());
    
    // 在同一连接上连续调用多个方法
    auto state = client.get_provision_state();
    EXPECT_EQ(state, ProvisionState::NONE);
    
    auto vin = client.read_vin();
    EXPECT_TRUE(vin.empty());
    
    auto binding = client.read_binding();
    EXPECT_TRUE(binding.vin.empty());
    
    state = client.get_provision_state();
    EXPECT_EQ(state, ProvisionState::NONE);
    
    client.disconnect();
}

TEST_F(IpcTest, LazyConnectOnFirstCall) {
    service_->initialize();
    startIpc();
    
    ProvClient client(socket_path_);
    // 不显式 connect，直接调用
    std::string vin = client.read_vin();
    EXPECT_TRUE(vin.empty());
    
    client.disconnect();
}

// ============================================================
// CR-007: readBinding 经 IPC 返回 sn（US-007）
// ============================================================
TEST_F(IpcTest, ReadBindingViaIpcReturnsSnAfterBinding) {
    // 加载配置（prov.sn + ecu.uid），供 ConfigTboxSnProvider 与 SeUidProvider 读取
    std::filesystem::path config_dir = std::string(test_dir_) + "/conf.d";
    std::filesystem::create_directories(config_dir);
    {
        std::ofstream file(std::string(test_dir_) + "/common.yaml");
        file << "common:\n  log:\n    type: console\n    path: ./log.txt\n";
    }
    {
        std::ofstream file(std::string(config_dir) + "/prov.yaml");
        file << "prov:\n  sn: \"TBOX-SN-IPC-TEST\"\n";
        file << "ecu:\n  uid: \"00000000000000000000000000000001\"\n";
    }
    CONFIG_MANAGER.load("prov", test_dir_);

    // 重新构造 service，使其使用新配置的 providers
    service_.reset();
    uid_provider_ = createSeUidProvider();
    sn_provider_ = createTboxSnProvider();
    service_ = std::make_unique<ProvService>(*store_, config_, *uid_provider_, *sn_provider_);

    service_->initialize();
    startIpc();
    
    ProvClient client(socket_path_);
    ASSERT_TRUE(client.connect());
    ASSERT_EQ(client.initialize(), ErrorCode::SUCCESS);
    
    // 写入合法 VIN 建立绑定
    ASSERT_EQ(client.write_vin("1HGBH41JXMN109186"), ErrorCode::SUCCESS);
    
    // 读取绑定：应包含 sn（由 ConfigTboxSnProvider 从 prov.sn 补齐）
    VehicleBinding binding = client.read_binding();
    EXPECT_EQ(binding.vin, "1HGBH41JXMN109186");
    EXPECT_EQ(binding.state, ProvisionState::BOUND);
    EXPECT_EQ(binding.sn, "TBOX-SN-IPC-TEST");
    EXPECT_NE(binding.sn, binding.ecu_uid);  // sn 与 ecu_uid 独立
    
    client.disconnect();
}
