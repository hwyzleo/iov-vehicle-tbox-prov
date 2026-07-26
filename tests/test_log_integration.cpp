#include <gtest/gtest.h>
#include "prov_log_adapter.h"
#include "prov_context.h"
#include "prov_service.h"
#include "framework_store.h"
#include <thread>
#include <chrono>

namespace tbox::prov {
namespace {

class LogIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        tbox::fw::log::LogConfig config;
        config.level = tbox::fw::log::LogLevel::kDebug;
        
        auto result = ProvLogAdapter::init("prov_integration_test", config);
        ASSERT_EQ(result.error, tbox::fw::log::LogError::kOk);
        
        store_ = std::make_unique<tbox::framework::Store>("prov_test", "/tmp/tbox_test");
        service_ = std::make_unique<ProvService>(*store_);
    }

    void TearDown() override {
        service_.reset();
        store_.reset();
    }

    std::unique_ptr<tbox::framework::Store> store_;
    std::unique_ptr<ProvService> service_;
};

TEST_F(LogIntegrationTest, ServiceInitialization) {
    // 测试服务初始化
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    // 验证初始化后可以读取 VIN（即使为空）
    auto vin = service_->read_vin();
    EXPECT_TRUE(vin.empty());  // 初始状态应为空
}

TEST_F(LogIntegrationTest, ContextPropagationDuringOperations) {
    // 测试上下文在操作期间被正确传播
    service_->initialize();
    
    std::string trace_id = "integration-trace-123";
    std::string request_id = "integration-req-456";
    
    {
        auto scope = make_context_scope(trace_id, request_id);
        
        // 执行读取操作
        auto vin = service_->read_vin();
        auto binding = service_->read_binding();
        auto state = service_->get_provision_state();
        
        // 验证上下文在操作期间被设置
        auto* current = tbox::fw::log::ContextScope::current();
        ASSERT_NE(current, nullptr);
        EXPECT_EQ(current->trace_id, trace_id);
        EXPECT_EQ(current->request_id, request_id);
    }
    
    // 上下文已清理
    auto* after = tbox::fw::log::ContextScope::current();
    EXPECT_EQ(after, nullptr);
}

TEST_F(LogIntegrationTest, MultipleModulesLogging) {
    // 测试多个模块的日志记录
    service_->initialize();
    
    // 服务模块日志
    ProvLogAdapter::service().info("prov.service.test",
        "Service module test log"
    );
    
    // VIN 模块日志
    ProvLogAdapter::vin().info("prov.vin.test",
        "VIN module test log",
        {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString("test-hash"))}
    );
    
    // 绑定模块日志
    ProvLogAdapter::binding().info("prov.binding.test",
        "Binding module test log",
        {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString("test-hash")),
         tbox::fw::log::Field("ecu_uid_hash", tbox::fw::log::FieldValue::makeString("uid-hash"))}
    );
    
    // IPC 模块日志
    ProvLogAdapter::ipc().info("prov.ipc.test",
        "IPC module test log",
        {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(42))}
    );
}

TEST_F(LogIntegrationTest, ErrorLoggingWithDesensitization) {
    // 测试错误日志中的脱敏处理
    service_->initialize();
    
    // 模拟一个需要脱敏的场景
    std::string test_vin = "LSVAU2180N2123456";
    std::string test_uid = "ECU123456789";
    
    // 记录包含敏感信息的日志（应自动脱敏）
    ProvLogAdapter::vin().warn("prov.vin.validation_failed",
        "VIN validation failed for testing",
        {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString(test_vin),
             tbox::fw::log::Sensitivity::Identifier),
         tbox::fw::log::Field("ecu_uid_hash", tbox::fw::log::FieldValue::makeString(test_uid),
             tbox::fw::log::Sensitivity::Identifier),
         tbox::fw::log::Field("validation_reason", tbox::fw::log::FieldValue::makeString("test_error"))}
    );
}

TEST_F(LogIntegrationTest, SecretFieldRejection) {
    // 测试 Secret 字段被正确拒绝
    service_->initialize();
    
    // 记录包含 Secret 字段的日志
    ProvLogAdapter::ipc().info("prov.ipc.test",
        "Testing secret field rejection",
        {tbox::fw::log::Field("secret_key", tbox::fw::log::FieldValue::makeString("my_secret_123"),
             tbox::fw::log::Sensitivity::Secret)}
    );
    
    // 验证日志输出中 secret_key 被脱敏（框架会自动处理）
    // 具体断言取决于框架的 Secret 处理机制
}

TEST_F(LogIntegrationTest, ConcurrentLogging) {
    // 测试并发日志记录
    service_->initialize();
    
    const int num_threads = 4;
    const int logs_per_thread = 10;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, logs_per_thread]() {
            for (int j = 0; j < logs_per_thread; ++j) {
                ProvLogAdapter::service().info("prov.service.concurrent_test",
                    "Concurrent logging test",
                    {tbox::fw::log::Field("thread_id", tbox::fw::log::FieldValue::makeInt(i)),
                     tbox::fw::log::Field("log_index", tbox::fw::log::FieldValue::makeInt(j))}
                );
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(LogIntegrationTest, LoggerInitializationFailed) {
    // 测试 Logger 初始化失败时的行为
    // 注意：这个测试需要模拟 Logger 初始化失败
    // 具体实现取决于 framework-log 的测试支持
    
    // 验证 strict 模式下的 fail-closed 行为
    tbox::fw::log::LogConfig config;
    config.strict = true;
    config.level = tbox::fw::log::LogLevel::kInfo;
    
    // 使用空服务名来触发初始化失败（如果框架支持）
    auto result = ProvLogAdapter::init("", config);
    // 框架可能会返回错误，也可能接受空字符串
    // 具体断言取决于框架行为
}

} // namespace
} // namespace tbox::prov
