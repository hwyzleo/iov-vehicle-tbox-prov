#include <gtest/gtest.h>
#include "prov_service.h"
#include <filesystem>

namespace tbox {
namespace prov {
namespace testing {

class ProvServiceTest : public ::testing::Test {
protected:
    std::string test_dir_ = "/tmp/prov_test_service";
    
    void SetUp() override {
        // 清理测试目录
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
        
        // 创建服务配置
        ProvServiceConfig config;
        config.storage_path = test_dir_;
        config.enable_write_protection = true;
        config.max_retry_count = 3;
        
        service = std::make_unique<ProvService>(config);
    }
    
    void TearDown() override {
        service.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::unique_ptr<ProvService> service;
};

TEST_F(ProvServiceTest, Initialize) {
    EXPECT_EQ(service->initialize(), ErrorCode::SUCCESS);
    EXPECT_TRUE(service->is_initialized());
}

TEST_F(ProvServiceTest, WriteVinWithValidFormat) {
    ASSERT_EQ(service->initialize(), ErrorCode::SUCCESS);
    
    // VIN格式正确时应该尝试写入（但会因为ECU UID读取失败而失败）
    EXPECT_EQ(service->write_vin("1HGBH41JXMN109186"), ErrorCode::ECU_UID_READ_ERROR);
}

TEST_F(ProvServiceTest, WriteVinWithInvalidFormat) {
    ASSERT_EQ(service->initialize(), ErrorCode::SUCCESS);
    
    // VIN格式错误时应该失败
    EXPECT_EQ(service->write_vin("INVALID_VIN"), ErrorCode::INVALID_VIN_FORMAT);
}

TEST_F(ProvServiceTest, ReadVinInitialState) {
    ASSERT_EQ(service->initialize(), ErrorCode::SUCCESS);
    
    // 初始状态应该返回空
    EXPECT_EQ(service->read_vin(), "");
}

TEST_F(ProvServiceTest, GetProvisionStateInitial) {
    ASSERT_EQ(service->initialize(), ErrorCode::SUCCESS);
    
    // 初始状态应该是NONE
    EXPECT_EQ(service->get_provision_state(), ProvisionState::NONE);
}

TEST_F(ProvServiceTest, ReadBindingInitial) {
    ASSERT_EQ(service->initialize(), ErrorCode::SUCCESS);
    
    // 初始状态应该返回空绑定
    VehicleBinding binding = service->read_binding();
    EXPECT_EQ(binding.vin, "");
    EXPECT_EQ(binding.ecu_uid, "");
    EXPECT_EQ(binding.state, ProvisionState::NONE);
}

TEST_F(ProvServiceTest, WriteVehicleConfigWithWriteProtection) {
    ASSERT_EQ(service->initialize(), ErrorCode::SUCCESS);
    
    // 写保护开启时写入配置应该失败
    std::vector<uint8_t> config_data = {0x01, 0x02, 0x03};
    EXPECT_EQ(service->write_vehicle_config(config_data), ErrorCode::SECURITY_ACCESS_NOT_GRANTED);
}

TEST_F(ProvServiceTest, WriteProductionInfoWithWriteProtection) {
    ASSERT_EQ(service->initialize(), ErrorCode::SUCCESS);
    
    // 写保护开启时写入生产信息应该失败
    ProductionInfo info;
    info.production_date = "2026-06-20";
    info.batch_num = "BATCH001";
    info.station_id = "STATION01";
    
    EXPECT_EQ(service->write_production_info(info), ErrorCode::SECURITY_ACCESS_NOT_GRANTED);
}

TEST_F(ProvServiceTest, AuthorizeRewriteWithValidFormat) {
    ASSERT_EQ(service->initialize(), ErrorCode::SUCCESS);
    
    // VIN格式正确时应该尝试重写（但会因为ECU UID读取失败而失败）
    EXPECT_EQ(service->authorize_rewrite("1HGBH41JXMN109186"), ErrorCode::ECU_UID_READ_ERROR);
}

TEST_F(ProvServiceTest, DoubleInitialize) {
    // 多次初始化应该成功
    EXPECT_EQ(service->initialize(), ErrorCode::SUCCESS);
    EXPECT_EQ(service->initialize(), ErrorCode::SUCCESS);
}

TEST_F(ProvServiceTest, OperationsBeforeInitialize) {
    // 未初始化时操作应该失败
    EXPECT_EQ(service->write_vin("1HGBH41JXMN109186"), ErrorCode::INVALID_STATE);
    EXPECT_EQ(service->read_vin(), "");
    EXPECT_EQ(service->get_provision_state(), ProvisionState::NONE);
}

} // namespace testing
} // namespace prov
} // namespace tbox