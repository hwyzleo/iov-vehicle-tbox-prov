#include <gtest/gtest.h>
#include "tbox_sn_provider.h"
#include "error_codes.h"
#include "config.h"
#include <fstream>
#include <filesystem>
#include <chrono>

namespace tbox {
namespace prov {
namespace testing {

class TboxSnProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/tbox_sn_test_" +
                    std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);

        // ConfigManager 需要 common.yaml（框架校验器要求 common.log 配置）
        config_dir_ = test_dir_ + "/conf.d";
        std::filesystem::create_directories(config_dir_);

        std::filesystem::path common_config = std::string(test_dir_) + "/common.yaml";
        {
            std::ofstream file(common_config);
            file << "# TBOX Common Configuration\n";
            file << "common:\n";
            file << "  log:\n";
            file << "    type: console\n";
            file << "    path: ./log.txt\n";
            file.close();
        }
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    /// 写入 prov.yaml 并加载框架配置
    void loadProvConfig(const std::string& yaml_content) {
        std::filesystem::path prov_config = std::string(config_dir_) + "/prov.yaml";
        {
            std::ofstream file(prov_config);
            file << yaml_content;
            file.close();
        }
        CONFIG_MANAGER.load("prov", test_dir_);
    }

    std::string test_dir_;
    std::string config_dir_;
};

// ============================================================
// ConfigTboxSnProvider
// ============================================================

TEST_F(TboxSnProviderTest, ConfigProviderReadSnSuccess) {
    loadProvConfig(
        "prov:\n"
        "  sn: \"TBOX-SN-12345\"\n");

    ConfigTboxSnProvider provider;
    auto result = provider.readSn();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.sn, "TBOX-SN-12345");
    EXPECT_EQ(result.sn_source, "config");
    EXPECT_EQ(result.environment, "test");
}

TEST_F(TboxSnProviderTest, ConfigProviderReadSnMissing) {
    // prov.sn 缺失（仅有 prov.ipc 等其他键）
    loadProvConfig(
        "prov:\n"
        "  ipc:\n"
        "    socket_path: \"/tmp/tbox-prov.sock\"\n");

    ConfigTboxSnProvider provider;
    auto result = provider.readSn();
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.sn.empty());
    EXPECT_EQ(result.failure_stage, "missing");
    EXPECT_EQ(result.sn_source, "config");
}

TEST_F(TboxSnProviderTest, ConfigProviderReadSnEmpty) {
    loadProvConfig(
        "prov:\n"
        "  sn: \"\"\n");

    ConfigTboxSnProvider provider;
    auto result = provider.readSn();
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.sn.empty());
    EXPECT_EQ(result.failure_stage, "empty");
}

TEST_F(TboxSnProviderTest, ConfigProviderDoesNotReadProvUid) {
    // 隔离验证：设置 prov.uid 但不设 prov.sn，ConfigTboxSnProvider 不得回退读取 prov.uid
    loadProvConfig(
        "prov:\n"
        "  uid: \"HSM-UID-FOR-TEST\"\n");

    ConfigTboxSnProvider provider;
    auto result = provider.readSn();
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.sn.empty());
    // 必须是 missing，不得返回 prov.uid 的值
    EXPECT_EQ(result.failure_stage, "missing");
    EXPECT_NE(result.sn, "HSM-UID-FOR-TEST");
}

TEST_F(TboxSnProviderTest, ConfigProviderSnIndependentFromUid) {
    // prov.sn 与 prov.uid 同时存在且不同，ConfigTboxSnProvider 只返回 prov.sn
    loadProvConfig(
        "prov:\n"
        "  uid: \"HSM-UID-FOR-TEST\"\n"
        "  sn: \"TBOX-SN-FOR-TEST\"\n");

    ConfigTboxSnProvider provider;
    auto result = provider.readSn();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.sn, "TBOX-SN-FOR-TEST");
    EXPECT_NE(result.sn, "HSM-UID-FOR-TEST");
}

// ============================================================
// PlatformTboxSnProvider
// ============================================================

TEST_F(TboxSnProviderTest, PlatformProviderReturnsUnavailable) {
    // 生产 Provider 桩实现：无可用平台 SN 权威来源，返回 SN 不可用
    PlatformTboxSnProvider provider;
    auto result = provider.readSn();
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.sn.empty());
    EXPECT_EQ(result.sn_source, "platform");
    EXPECT_EQ(result.environment, "production");
}

TEST_F(TboxSnProviderTest, PlatformProviderDoesNotReadConfig) {
    // 即使配置中存在 prov.sn，生产 Provider 也不得读取它
    loadProvConfig(
        "prov:\n"
        "  sn: \"TBOX-SN-FOR-TEST\"\n");

    PlatformTboxSnProvider provider;
    auto result = provider.readSn();
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.sn.empty());
    EXPECT_NE(result.sn, "TBOX-SN-FOR-TEST");
    EXPECT_EQ(result.sn_source, "platform");
}

// ============================================================
// 工厂选择
// ============================================================

TEST_F(TboxSnProviderTest, FactoryReturnsConfigProviderInTestBuild) {
    // 当前为测试构建（TEST_ENVIRONMENT 已定义），工厂应返回 ConfigTboxSnProvider
    loadProvConfig(
        "prov:\n"
        "  sn: \"FACTORY-SN-TEST\"\n");

    auto provider = createTboxSnProvider();
    ASSERT_NE(provider, nullptr);
    auto result = provider->readSn();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.sn, "FACTORY-SN-TEST");
    EXPECT_EQ(result.sn_source, "config");
}

// ============================================================
// 标识隔离：sn 与 ecu_uid 相同/不同/分别缺失
// ============================================================

TEST_F(TboxSnProviderTest, SnAndEcuUidSemanticallyIndependent) {
    // sn 与 ecu_uid 相同时仍按独立语义处理（ConfigTboxSnProvider 只读 prov.sn）
    loadProvConfig(
        "prov:\n"
        "  uid: \"SAME-VALUE\"\n"
        "  sn: \"SAME-VALUE\"\n");

    ConfigTboxSnProvider sn_provider;
    auto sn_result = sn_provider.readSn();
    EXPECT_TRUE(sn_result.success);
    EXPECT_EQ(sn_result.sn, "SAME-VALUE");
    // sn 来源是 config/prov.sn，与 ecu_uid 的读取路径无关
    EXPECT_EQ(sn_result.sn_source, "config");
}

} // namespace testing
} // namespace prov
} // namespace tbox
