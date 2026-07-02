#include <gtest/gtest.h>
#include "ecu_uid.h"
#include "error_codes.h"
#include "config.h"
#include <fstream>
#include <filesystem>
#include <chrono>

namespace tbox {
namespace prov {
namespace testing {

class EcuUidTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时测试目录
        test_dir_ = "/tmp/ecu_uid_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        // 加载框架配置（测试环境需要先加载配置才能读取UID）
        CONFIG_MANAGER.load("prov");
    }

    void TearDown() override {
        // 清理测试目录
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(EcuUidTest, ReadUidSuccess) {
    // 测试正常读取UID
    std::string uid = EcuUid::read_uid();
    EXPECT_FALSE(uid.empty());
    EXPECT_TRUE(EcuUid::validate(uid));
}

TEST_F(EcuUidTest, ReadUidDetailedSuccess) {
    // 测试详细读取结果
    auto result = EcuUid::read_uid_detailed();
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.uid.empty());
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_EQ(result.error_code, ErrorCode::SUCCESS);
}

TEST_F(EcuUidTest, ValidateUidFormat) {
    // 测试UID格式验证
    EXPECT_TRUE(EcuUid::validate("0123456789ABCDEF"));
    EXPECT_TRUE(EcuUid::validate("SE987654321"));
    EXPECT_FALSE(EcuUid::validate(""));
    EXPECT_FALSE(EcuUid::validate("123"));  // 太短
}

TEST_F(EcuUidTest, IsSeHardwarePresent) {
    // 测试SE硬件检测
    // 注意：这个测试依赖于实际硬件环境
    bool present = EcuUid::is_se_hardware_present();
    // 在测试环境中，我们假设SE存在
    EXPECT_TRUE(present);
}

TEST_F(EcuUidTest, IsTestEnvironment) {
    // 测试环境检测
    // 注意：这个测试依赖于编译开关 TEST_ENVIRONMENT
    bool is_test = EcuUid::is_test_environment();
    // 验证测试环境标志已正确设置
    EXPECT_TRUE(is_test);
}

TEST_F(EcuUidTest, ReadUidFromConfigSuccess) {
    // 测试从框架配置读取UID
    // 需要先加载配置
    auto err = CONFIG_MANAGER.load("prov");
    ASSERT_EQ(err, hwyz::config::ConfigError::kOk);
    
    // 调用被测函数
    auto result = EcuUid::read_uid_from_config();
    
    // 验证结果（配置文件中应包含 ecu.uid）
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
    EXPECT_TRUE(EcuUid::validate(result.value()));
}

TEST_F(EcuUidTest, ReadUidFromConfigWithCustomValue) {
    // 测试从框架配置读取自定义UID值
    // 创建临时配置目录和文件
    std::filesystem::path config_dir = test_dir_ + "/conf.d";
    std::filesystem::create_directories(config_dir);
    
    std::filesystem::path config_file = config_dir / "prov.yaml";
    {
        std::ofstream file(config_file);
        file << "ecu:\n";
        file << "  uid: \"CUSTOM_UID_12345\"\n";
        file.close();
    }
    
    // 使用自定义配置根目录加载
    auto err = CONFIG_MANAGER.load("prov", test_dir_);
    ASSERT_EQ(err, hwyz::config::ConfigError::kOk);
    
    // 调用被测函数
    auto result = EcuUid::read_uid_from_config();
    
    // 验证结果
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "CUSTOM_UID_12345");
}

// PROV-1008 测试：SE读取失败/超时
// 注意：当前架构无法实现此测试，因为：
// 1. is_se_hardware_present() 是静态方法，硬编码返回 false
// 2. read_from_se() 是静态方法，硬编码返回 "SE987654321"
// 3. 没有依赖注入机制来 mock 这些方法
// 要实现此测试，需要重构 EcuUid 类引入虚拟方法或测试钩子
// TEST_F(EcuUidTest, ReadUidSeReadFailed) {
//     auto result = EcuUid::read_uid_detailed();
//     EXPECT_FALSE(result.success);
//     EXPECT_EQ(result.error_code, ErrorCode::SE_UID_READ_FAILED);
// }

// PROV-1010 测试：生产环境无SE
// 注意：当前架构无法实现此测试，因为：
// 1. is_se_hardware_present() 是静态方法，硬编码返回 false
// 2. is_test_environment() 依赖编译开关 TEST_ENVIRONMENT（当前未定义）
// 3. 没有依赖注入机制来 mock 这些方法
// 要实现此测试，需要：
//   - 在 CMakeLists.txt 中添加 TEST_ENVIRONMENT 编译选项用于测试
//   - 或重构 EcuUid 类引入虚拟方法或测试钩子
// TEST_F(EcuUidTest, ReadUidProductionEnvNoSe) {
//     auto result = EcuUid::read_uid_detailed();
//     EXPECT_FALSE(result.success);
//     EXPECT_EQ(result.error_code, ErrorCode::SE_MISSING_PRODUCTION_FAIL_CLOSED);
// }

} // namespace testing
} // namespace prov
} // namespace tbox
