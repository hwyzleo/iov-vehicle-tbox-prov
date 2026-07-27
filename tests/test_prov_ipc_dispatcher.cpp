#include <gtest/gtest.h>
#include "prov_ipc_dispatcher.h"
#include "prov_service.h"
#include "framework_store.h"
#include "ipc_protocol.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <chrono>
#include <thread>
#include <memory>

using json = nlohmann::json;

class ProvIpcDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/prov_dispatcher_test_" +
                     std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);

        store_ = std::make_unique<tbox::framework::Store>("prov", test_dir_);
        config_.enable_write_protection = true;
        config_.max_retry_count = 3;
        config_.ipc_socket_path = test_dir_ + "/test.sock";

        service_ = std::make_unique<tbox::prov::ProvService>(*store_, config_);
        service_->initialize();
        dispatcher_ = std::make_unique<tbox::prov::ProvIpcDispatcher>(service_.get());
    }

    void TearDown() override {
        dispatcher_.reset();
        service_->stop_ipc_server();
        service_.reset();
        store_.reset();
        std::filesystem::remove_all(test_dir_);
    }

    /// 辅助：调用 dispatcher 并解析响应 JSON
    json dispatch(uint32_t method_id, const std::string& params = "{}") {
        std::string response = dispatcher_->dispatch(method_id, params, 0);
        return json::parse(response);
    }

    std::string test_dir_;
    std::unique_ptr<tbox::framework::Store> store_;
    tbox::prov::ProvServiceConfig config_;
    std::unique_ptr<tbox::prov::ProvService> service_;
    std::unique_ptr<tbox::prov::ProvIpcDispatcher> dispatcher_;
};

// ============================================================
// Method 映射测试
// ============================================================

TEST_F(ProvIpcDispatcherTest, InitializeReturnsStatus) {
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::INITIALIZE));
    EXPECT_TRUE(j.contains("status"));
    EXPECT_TRUE(j.contains("success"));
    EXPECT_EQ(j["status"].get<int32_t>(), 0);
    EXPECT_TRUE(j["success"].get<bool>());
}

TEST_F(ProvIpcDispatcherTest, ReadVinReturnsVinField) {
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::READ_VIN));
    EXPECT_TRUE(j.contains("status"));
    EXPECT_TRUE(j.contains("vin"));
    EXPECT_EQ(j["status"].get<int32_t>(), 0);
    EXPECT_TRUE(j["vin"].get<std::string>().empty());
}

TEST_F(ProvIpcDispatcherTest, ReadBindingReturnsAllFields) {
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::READ_BINDING));
    EXPECT_TRUE(j.contains("status"));
    EXPECT_TRUE(j.contains("vin"));
    EXPECT_TRUE(j.contains("ecu_uid"));
    EXPECT_TRUE(j.contains("state"));
    EXPECT_TRUE(j.contains("locked"));
    EXPECT_EQ(j["status"].get<int32_t>(), 0);
    EXPECT_EQ(j["state"].get<int>(), static_cast<int>(tbox::prov::ProvisionState::NONE));
}

TEST_F(ProvIpcDispatcherTest, GetProvisionStateReturnsStateField) {
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::GET_PROVISION_STATE));
    EXPECT_TRUE(j.contains("status"));
    EXPECT_TRUE(j.contains("state"));
    EXPECT_EQ(j["status"].get<int32_t>(), 0);
    EXPECT_EQ(j["state"].get<int>(), static_cast<int>(tbox::prov::ProvisionState::NONE));
}

TEST_F(ProvIpcDispatcherTest, WriteVinWithMissingParamReturnsError) {
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::WRITE_VIN), "{}");
    EXPECT_TRUE(j.contains("status"));
    EXPECT_FALSE(j["success"].get<bool>());
    // 缺少 vin 参数应返回业务错误码
    EXPECT_NE(j["status"].get<int32_t>(), 0);
}

TEST_F(ProvIpcDispatcherTest, AuthorizeRewriteWithMissingParamReturnsError) {
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::AUTHORIZE_REWRITE), "{}");
    EXPECT_TRUE(j.contains("status"));
    EXPECT_FALSE(j["success"].get<bool>());
    EXPECT_NE(j["status"].get<int32_t>(), 0);
}

TEST_F(ProvIpcDispatcherTest, WriteVehicleConfigReturnsNotImplemented) {
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::WRITE_VEHICLE_CONFIG));
    EXPECT_TRUE(j.contains("status"));
    EXPECT_FALSE(j["success"].get<bool>());
    EXPECT_NE(j["status"].get<int32_t>(), 0);
}

TEST_F(ProvIpcDispatcherTest, WriteProductionInfoReturnsNotImplemented) {
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::WRITE_PRODUCTION_INFO));
    EXPECT_TRUE(j.contains("status"));
    EXPECT_FALSE(j["success"].get<bool>());
    EXPECT_NE(j["status"].get<int32_t>(), 0);
}

// ============================================================
// 未知 method / 异常处理测试
// ============================================================

TEST_F(ProvIpcDispatcherTest, UnknownMethodReturnsError) {
    auto j = dispatch(9999);
    EXPECT_TRUE(j.contains("status"));
    // FW-0306 = 306
    EXPECT_EQ(j["status"].get<int32_t>(), 306);
    EXPECT_TRUE(j.contains("error"));
}

TEST_F(ProvIpcDispatcherTest, ZeroMethodIdReturnsError) {
    auto j = dispatch(0);
    EXPECT_EQ(j["status"].get<int32_t>(), 306);
}

// ============================================================
// JSON 参数解析测试
// ============================================================

TEST_F(ProvIpcDispatcherTest, WriteVinWithValidVinParamParsesCorrectly) {
    // 使用合法 VIN（虽然写入可能因写保护失败，但参数应被正确解析）
    std::string params = json({{"vin", "1HGCM82633A123456"}}).dump();
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::WRITE_VIN), params);
    // 应包含 status 和 success 字段（即使业务失败，参数解析应成功）
    EXPECT_TRUE(j.contains("status"));
    EXPECT_TRUE(j.contains("success"));
}

TEST_F(ProvIpcDispatcherTest, WriteVinWithInvalidJsonReturnsError) {
    // 非法 JSON 字符串
    auto j = dispatch(static_cast<uint32_t>(tbox::prov::ipc::MethodId::WRITE_VIN), "not a json");
    // extract_string_field 会捕获异常并返回空，然后 handle_write_vin 返回 INVALID_VIN_FORMAT
    EXPECT_TRUE(j.contains("status"));
    EXPECT_NE(j["status"].get<int32_t>(), 0);
}

// ============================================================
// 响应格式一致性测试
// ============================================================

TEST_F(ProvIpcDispatcherTest, AllResponsesContainStatusField) {
    // 所有 method 的响应都应包含 status 字段
    uint32_t methods[] = {
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::INITIALIZE),
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::READ_VIN),
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::READ_BINDING),
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::GET_PROVISION_STATE),
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::WRITE_VEHICLE_CONFIG),
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::WRITE_PRODUCTION_INFO),
    };

    for (uint32_t m : methods) {
        std::string response = dispatcher_->dispatch(m, "{}", 0);
        auto j = json::parse(response);
        EXPECT_TRUE(j.contains("status")) << "Method " << m << " response missing status field";
    }
}

TEST_F(ProvIpcDispatcherTest, ResponseIsValidJson) {
    // 所有 method 的响应都应是合法 JSON
    uint32_t methods[] = {
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::INITIALIZE),
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::READ_VIN),
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::READ_BINDING),
        static_cast<uint32_t>(tbox::prov::ipc::MethodId::GET_PROVISION_STATE),
        9999,  // unknown method
    };

    for (uint32_t m : methods) {
        std::string response = dispatcher_->dispatch(m, "{}", 0);
        EXPECT_NO_THROW({
            json::parse(response);
        }) << "Method " << m << " response is not valid JSON";
    }
}
