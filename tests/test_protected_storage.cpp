#include <gtest/gtest.h>
#include "protected_storage_impl.h"
#include <filesystem>
#include <cstdio>

namespace tbox {
namespace prov {
namespace testing {

class ProtectedStorageTest : public ::testing::Test {
protected:
    std::string test_dir_ = "/tmp/prov_test_storage";
    
    void SetUp() override {
        // 清理测试目录
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // 清理测试目录
        std::filesystem::remove_all(test_dir_);
    }
};

TEST_F(ProtectedStorageTest, Initialize) {
    ProtectedStorageImpl storage(test_dir_);
    EXPECT_EQ(storage.initialize(), ErrorCode::SUCCESS);
}

TEST_F(ProtectedStorageTest, ReadWriteVehicleBinding) {
    ProtectedStorageImpl storage(test_dir_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);
    
    // 创建测试数据
    VehicleBinding binding;
    binding.vin = "1HGBH41JXMN109186";
    binding.ecu_uid = "ECU123456789";
    binding.state = ProvisionState::BOUND;
    binding.locked = true;
    binding.retry_count = 2;
    binding.rewrite_count = 1;
    
    // 写入
    EXPECT_EQ(storage.write_vehicle_binding(binding), ErrorCode::SUCCESS);
    
    // 读取
    auto result = storage.read_vehicle_binding();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->vin, binding.vin);
    EXPECT_EQ(result->ecu_uid, binding.ecu_uid);
    EXPECT_EQ(result->state, binding.state);
    EXPECT_EQ(result->locked, binding.locked);
    EXPECT_EQ(result->retry_count, binding.retry_count);
    EXPECT_EQ(result->rewrite_count, binding.rewrite_count);
}

TEST_F(ProtectedStorageTest, ReadWriteVehicleConfig) {
    ProtectedStorageImpl storage(test_dir_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);
    
    // 创建测试数据
    VehicleConfig config;
    config.variant_coding = {0x01, 0x02, 0x03, 0x04};
    config.verified = true;
    
    // 写入
    EXPECT_EQ(storage.write_vehicle_config(config), ErrorCode::SUCCESS);
    
    // 读取
    auto result = storage.read_vehicle_config();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->variant_coding, config.variant_coding);
    EXPECT_EQ(result->verified, config.verified);
}

TEST_F(ProtectedStorageTest, ReadWriteProductionInfo) {
    ProtectedStorageImpl storage(test_dir_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);
    
    // 创建测试数据
    ProductionInfo info;
    info.production_date = "2026-06-20";
    info.batch_num = "BATCH001";
    info.station_id = "STATION01";
    
    // 写入
    EXPECT_EQ(storage.write_production_info(info), ErrorCode::SUCCESS);
    
    // 读取
    auto result = storage.read_production_info();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->production_date, info.production_date);
    EXPECT_EQ(result->batch_num, info.batch_num);
    EXPECT_EQ(result->station_id, info.station_id);
}

TEST_F(ProtectedStorageTest, WriteProtection) {
    ProtectedStorageImpl storage(test_dir_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);
    
    // 初始状态未锁定
    EXPECT_FALSE(storage.is_write_protected());
    
    // 设置写保护
    EXPECT_EQ(storage.set_write_protection(true), ErrorCode::SUCCESS);
    EXPECT_TRUE(storage.is_write_protected());
    
    // 尝试写入（应该失败）
    VehicleBinding binding;
    binding.vin = "1HGBH41JXMN109186";
    EXPECT_EQ(storage.write_vehicle_binding(binding), ErrorCode::SECURITY_ACCESS_NOT_GRANTED);
    
    // 解除写保护
    EXPECT_EQ(storage.set_write_protection(false), ErrorCode::SUCCESS);
    EXPECT_FALSE(storage.is_write_protected());
    
    // 再次写入（应该成功）
    EXPECT_EQ(storage.write_vehicle_binding(binding), ErrorCode::SUCCESS);
}

TEST_F(ProtectedStorageTest, ClearAll) {
    ProtectedStorageImpl storage(test_dir_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);
    
    // 写入数据
    VehicleBinding binding;
    binding.vin = "1HGBH41JXMN109186";
    storage.write_vehicle_binding(binding);
    
    // 清除所有数据
    EXPECT_EQ(storage.clear_all(), ErrorCode::SUCCESS);
    
    // 验证数据已清除
    auto result = storage.read_vehicle_binding();
    EXPECT_FALSE(result.has_value());
}

TEST_F(ProtectedStorageTest, NonExistentData) {
    ProtectedStorageImpl storage(test_dir_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);
    
    // 读取不存在的数据
    auto binding = storage.read_vehicle_binding();
    EXPECT_FALSE(binding.has_value());
    
    auto config = storage.read_vehicle_config();
    EXPECT_FALSE(config.has_value());
    
    auto info = storage.read_production_info();
    EXPECT_FALSE(info.has_value());
}

} // namespace testing
} // namespace prov
} // namespace tbox