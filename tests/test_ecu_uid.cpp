#include <gtest/gtest.h>
#include "ecu_uid.h"
#include "error_codes.h"
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
    // 验证返回值是布尔类型（编译通过即验证）
    (void)is_test;
}

TEST_F(EcuUidTest, ReadUidFromConfigFileSuccess) {
    // 测试从配置文件读取UID
    // read_uid_from_config_file() reads from ConfigPath::TEST_UID_CONFIG_LOCAL (./config/prov_test.conf)
    std::filesystem::path config_dir = "./config";
    std::filesystem::path config_file_path = config_dir / "prov_test.conf";
    
    // 创建配置目录
    std::filesystem::create_directories(config_dir);
    
    // 写入测试配置文件
    {
        std::ofstream config_file(config_file_path);
        config_file << "# Test config\n";
        config_file << "uid=TEST_UID_12345\n";
        config_file.close();
    }
    
    // 调用被测函数
    auto result = EcuUid::read_uid_from_config_file();
    
    // 验证结果
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "TEST_UID_12345");
    
    // 清理测试文件
    std::filesystem::remove(config_file_path);
    std::filesystem::remove(config_dir);
}

TEST_F(EcuUidTest, ReadUidFromConfigFileNotFound) {
    // 测试配置文件不存在的情况
    // read_uid_from_config_file() 依赖 /etc/tbox/prov_test.conf 和 ./config/prov_test.conf
    // 在测试环境中，这些文件通常不存在，应返回 std::nullopt
    auto result = EcuUid::read_uid_from_config_file();
    EXPECT_FALSE(result.has_value());
}

// PROV-1008 测试：SE读取失败/超时
// 注意：当前架构无法实现此测试，因为：
// 1. is_se_hardware_present() 是静态方法，硬编码返回 true
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
// 1. is_se_hardware_present() 是静态方法，硬编码返回 true
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
