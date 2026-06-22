#include <gtest/gtest.h>
#include "data_models.h"
#include <chrono>

namespace tbox {
namespace prov {
namespace testing {

class DataModelsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DataModelsTest, ProvisionStateEnum) {
    // 测试状态枚举值
    EXPECT_EQ(static_cast<uint8_t>(ProvisionState::NONE), 0);
    EXPECT_EQ(static_cast<uint8_t>(ProvisionState::VIN_WRITTEN), 1);
    EXPECT_EQ(static_cast<uint8_t>(ProvisionState::BOUND), 2);
    EXPECT_EQ(static_cast<uint8_t>(ProvisionState::FAILED), 3);
}

TEST_F(DataModelsTest, VehicleBindingDefault) {
    // 测试默认构造
    VehicleBinding binding;
    EXPECT_EQ(binding.vin, "");
    EXPECT_EQ(binding.ecu_uid, "");
    EXPECT_EQ(binding.state, ProvisionState::NONE);
    EXPECT_FALSE(binding.locked);
    EXPECT_EQ(binding.retry_count, 0);
    EXPECT_EQ(binding.rewrite_count, 0);
}

TEST_F(DataModelsTest, VehicleBindingAssignment) {
    // 测试赋值
    VehicleBinding binding;
    binding.vin = "1HGBH41JXMN109186";
    binding.ecu_uid = "ECU123456789";
    binding.state = ProvisionState::BOUND;
    binding.locked = true;
    binding.retry_count = 2;
    binding.rewrite_count = 1;
    
    EXPECT_EQ(binding.vin, "1HGBH41JXMN109186");
    EXPECT_EQ(binding.ecu_uid, "ECU123456789");
    EXPECT_EQ(binding.state, ProvisionState::BOUND);
    EXPECT_TRUE(binding.locked);
    EXPECT_EQ(binding.retry_count, 2);
    EXPECT_EQ(binding.rewrite_count, 1);
}

TEST_F(DataModelsTest, VehicleConfigDefault) {
    // 测试默认构造
    VehicleConfig config;
    EXPECT_TRUE(config.variant_coding.empty());
    EXPECT_FALSE(config.verified);
}

TEST_F(DataModelsTest, ProductionInfoDefault) {
    // 测试默认构造
    ProductionInfo info;
    EXPECT_EQ(info.production_date, "");
    EXPECT_EQ(info.batch_num, "");
    EXPECT_EQ(info.station_id, "");
}

TEST_F(DataModelsTest, VinConstraints) {
    // 测试VIN约束常量
    EXPECT_EQ(VinConstraints::VIN_LENGTH, 17);
    EXPECT_EQ(VinConstraints::CHECK_DIGIT_POSITION, 9);
    EXPECT_STREQ(VinConstraints::EXCLUDED_CHARS, "IOQ");
}

} // namespace testing
} // namespace prov
} // namespace tbox