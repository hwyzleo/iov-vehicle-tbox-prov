#include <gtest/gtest.h>
#include "prov_log_adapter.h"

namespace tbox::prov {
namespace {

class ProvLogAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        tbox::fw::log::LogConfig config;
        config.level = tbox::fw::log::LogLevel::kDebug;
        
        auto result = ProvLogAdapter::init("prov_test", config);
        ASSERT_EQ(result.error, tbox::fw::log::LogError::kOk);
    }
};

TEST_F(ProvLogAdapterTest, InitSucceeds) {
    // SetUp 已验证初始化成功
    SUCCEED();
}

TEST_F(ProvLogAdapterTest, GetServiceLogger) {
    auto logger = ProvLogAdapter::service();
    // Logger 实例应该可以调用（不崩溃）
    logger.info("test.event", "test message");
}

TEST_F(ProvLogAdapterTest, GetVinLogger) {
    auto logger = ProvLogAdapter::vin();
    logger.info("test.event", "test message");
}

TEST_F(ProvLogAdapterTest, GetBindingLogger) {
    auto logger = ProvLogAdapter::binding();
    logger.info("test.event", "test message");
}

TEST_F(ProvLogAdapterTest, GetRewriteLogger) {
    auto logger = ProvLogAdapter::rewrite();
    logger.info("test.event", "test message");
}

TEST_F(ProvLogAdapterTest, GetVehicleConfigLogger) {
    auto logger = ProvLogAdapter::vehicle_config();
    logger.info("test.event", "test message");
}

TEST_F(ProvLogAdapterTest, GetProductionLogger) {
    auto logger = ProvLogAdapter::production();
    logger.info("test.event", "test message");
}

TEST_F(ProvLogAdapterTest, GetIpcLogger) {
    auto logger = ProvLogAdapter::ipc();
    logger.info("test.event", "test message");
}

} // namespace
} // namespace tbox::prov
