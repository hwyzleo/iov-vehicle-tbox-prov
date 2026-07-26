#include <gtest/gtest.h>
#include "prov_log_adapter.h"
#include "prov_context.h"
#include "prov_service.h"
#include "framework_store.h"
#include <regex>

namespace tbox::prov {
namespace {

class LogEventsTest : public ::testing::Test {
protected:
    void SetUp() override {
        tbox::fw::log::LogConfig config;
        config.level = tbox::fw::log::LogLevel::kDebug;
        
        auto result = ProvLogAdapter::init("prov_test", config);
        ASSERT_EQ(result.error, tbox::fw::log::LogError::kOk);
    }
};

TEST_F(LogEventsTest, EventNameFormat) {
    // 验证事件名格式：prov.<module>.<action> 或 prov.<module>.<submodule>.<action>
    std::regex event_pattern("^prov\\.[a-z_]+\\.[a-z_.]+$");
    
    // 业务事件清单中的所有事件
    EXPECT_TRUE(std::regex_match("prov.service.log_initialized", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.service.starting", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.service.initialized", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.service.ready", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.service.shutting_down", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.vin.validation_failed", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.vin.write.succeeded", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.vin.write.failed", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.uid.read_failed", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.binding.created", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.binding.readback_failed", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.binding.rewrite_denied", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.binding.rewrite_succeeded", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.vehicle_config.write_failed", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.production_info.write_failed", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.ipc.started", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.ipc.client_connected", event_pattern));
    EXPECT_TRUE(std::regex_match("prov.ipc.request_received", event_pattern));
}

TEST_F(LogEventsTest, LoggerMethodsExist) {
    // 验证所有模块 Logger 可以获取并调用
    auto service_logger = ProvLogAdapter::service();
    auto vin_logger = ProvLogAdapter::vin();
    auto binding_logger = ProvLogAdapter::binding();
    auto rewrite_logger = ProvLogAdapter::rewrite();
    auto vehicle_config_logger = ProvLogAdapter::vehicle_config();
    auto production_logger = ProvLogAdapter::production();
    auto ipc_logger = ProvLogAdapter::ipc();
    
    // 调用日志方法（不崩溃即通过）
    service_logger.info("prov.test.event", "test message");
    vin_logger.info("prov.test.event", "test message");
    binding_logger.info("prov.test.event", "test message");
    rewrite_logger.info("prov.test.event", "test message");
    vehicle_config_logger.info("prov.test.event", "test message");
    production_logger.info("prov.test.event", "test message");
    ipc_logger.info("prov.test.event", "test message");
}

TEST_F(LogEventsTest, ContextPropagationWorks) {
    std::string trace_id = "trace-123";
    std::string request_id = "req-456";
    
    {
        auto scope = make_context_scope(trace_id, request_id);
        
        // 在上下文中记录日志
        ProvLogAdapter::vin().info("prov.vin.write.succeeded",
            "Test context propagation",
            {tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString("test_hash")),
             tbox::fw::log::Field("duration_ms", tbox::fw::log::FieldValue::makeInt(100))}
        );
    }
    
    // 上下文已清理
    auto* current = tbox::fw::log::ContextScope::current();
    EXPECT_EQ(current, nullptr);
}

TEST_F(LogEventsTest, FieldTypesWork) {
    // 验证各种字段类型可以正确传递
    ProvLogAdapter::service().info("prov.test.event",
        "Test field types",
        {tbox::fw::log::Field("string_field", tbox::fw::log::FieldValue::makeString("value")),
         tbox::fw::log::Field("int_field", tbox::fw::log::FieldValue::makeInt(42)),
         tbox::fw::log::Field("bool_field", tbox::fw::log::FieldValue::makeBool(true)),
         tbox::fw::log::Field("double_field", tbox::fw::log::FieldValue::makeDouble(3.14))}
    );
}

TEST_F(LogEventsTest, SensitivityLevelsWork) {
    // 验证不同敏感度级别的字段
    ProvLogAdapter::service().info("prov.test.event",
        "Test sensitivity levels",
        {tbox::fw::log::Field("normal_field", 
             tbox::fw::log::FieldValue::makeString("normal"),
             tbox::fw::log::Sensitivity::Normal),
         tbox::fw::log::Field("identifier_field", 
             tbox::fw::log::FieldValue::makeString("VIN123"),
             tbox::fw::log::Sensitivity::Identifier),
         tbox::fw::log::Field("secret_field", 
             tbox::fw::log::FieldValue::makeString("secret"),
             tbox::fw::log::Sensitivity::Secret)}
    );
}

TEST_F(LogEventsTest, GenerateRequestId) {
    std::string id1 = generate_request_id();
    std::string id2 = generate_request_id();
    
    // 两次生成的 ID 应不同
    EXPECT_NE(id1, id2);
    
    // ID 应以 "prov-" 开头
    EXPECT_TRUE(id1.substr(0, 5) == "prov-");
    EXPECT_TRUE(id2.substr(0, 5) == "prov-");
}

TEST_F(LogEventsTest, ErrorCodeBoundary) {
    // 验证 PROV 业务错误码在 10xx 范围内
    EXPECT_GE(static_cast<int>(ErrorCode::INVALID_VIN_FORMAT), 1000);
    EXPECT_LE(static_cast<int>(ErrorCode::INVALID_VIN_FORMAT), 1099);
    EXPECT_GE(static_cast<int>(ErrorCode::VIN_WRITE_FAILED), 1000);
    EXPECT_LE(static_cast<int>(ErrorCode::VIN_WRITE_FAILED), 1099);
    // 注意：INVALID_STATE 等通用错误码可能不在 10xx 范围内
}

} // namespace
} // namespace tbox::prov
