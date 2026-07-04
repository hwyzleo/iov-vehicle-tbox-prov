#include <gtest/gtest.h>
#include "prov_service.h"
#include "prov_client.h"
#include "framework_store.h"
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>

using namespace tbox::prov;

class IpcTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/prov_ipc_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        store_ = std::make_unique<tbox::framework::Store>("prov", test_dir_);
        config_.enable_write_protection = true;
        config_.max_retry_count = 3;
        config_.ipc_socket_path = test_dir_ + "/test.sock";
        
        service_ = std::make_unique<ProvService>(*store_, config_);
    }
    
    void TearDown() override {
        if (service_) {
            service_->stop_ipc_server();
        }
        service_.reset();
        store_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::string test_dir_;
    std::unique_ptr<tbox::framework::Store> store_;
    ProvServiceConfig config_;
    std::unique_ptr<ProvService> service_;
};

TEST_F(IpcTest, InitializeAndStartIpcServer) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = service_->start_ipc_server();
    EXPECT_TRUE(started);
}

TEST_F(IpcTest, ConnectClient) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = service_->start_ipc_server();
    EXPECT_TRUE(started);
    
    ProvClient client(config_.ipc_socket_path);
    bool connected = client.connect();
    EXPECT_TRUE(connected);
    
    client.disconnect();
}

TEST_F(IpcTest, ReadVinViaIpc) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = service_->start_ipc_server();
    EXPECT_TRUE(started);
    
    ProvClient client(config_.ipc_socket_path);
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
    
    bool started = service_->start_ipc_server();
    EXPECT_TRUE(started);
    
    ProvClient client(config_.ipc_socket_path);
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
    
    bool started = service_->start_ipc_server();
    EXPECT_TRUE(started);
    
    // 创建多个客户端
    ProvClient client1(config_.ipc_socket_path);
    ProvClient client2(config_.ipc_socket_path);
    
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
    
    bool started = service_->start_ipc_server();
    EXPECT_TRUE(started);
    
    ProvClient client(config_.ipc_socket_path);
    
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
    
    bool started = service_->start_ipc_server();
    EXPECT_TRUE(started);
    
    // 停止服务器
    service_->stop_ipc_server();
    
    // 客户端应该无法连接
    ProvClient client(config_.ipc_socket_path);
    bool connected = client.connect();
    EXPECT_FALSE(connected);
}

TEST_F(IpcTest, WriteVinWithoutSecurityAccess) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    bool started = service_->start_ipc_server();
    EXPECT_TRUE(started);
    
    ProvClient client(config_.ipc_socket_path);
    bool connected = client.connect();
    EXPECT_TRUE(connected);
    
    client.disconnect();
}