#include <gtest/gtest.h>
#include "prov_service.h"
#include <filesystem>
#include <chrono>

using namespace tbox::prov;

class ProvServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/prov_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        config_.storage_path = test_dir_;
        config_.enable_write_protection = true;
        config_.max_retry_count = 3;
        
        service_ = std::make_unique<ProvService>(config_);
    }
    
    void TearDown() override {
        service_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::string test_dir_;
    ProvServiceConfig config_;
    std::unique_ptr<ProvService> service_;
};

TEST_F(ProvServiceTest, InitializeSuccess) {
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    EXPECT_TRUE(service_->is_initialized());
}

TEST_F(ProvServiceTest, InitializeAlreadyInitialized) {
    service_->initialize();
    auto result = service_->initialize();
    EXPECT_EQ(result, ErrorCode::SUCCESS);
}

TEST_F(ProvServiceTest, WriteVinSuccess) {
    service_->initialize();
    
    std::string vin = "1HGBH41JXMN109186";
    auto result = service_->write_vin(vin);
    EXPECT_EQ(result, ErrorCode::SUCCESS);
    
    auto read_vin = service_->read_vin();
    EXPECT_EQ(read_vin, vin);
}

TEST_F(ProvServiceTest, WriteVinInvalidFormat) {
    service_->initialize();
    
    std::string invalid_vin = "INVALID_VIN";
    auto result = service_->write_vin(invalid_vin);
    EXPECT_EQ(result, ErrorCode::INVALID_VIN_FORMAT);
}

TEST_F(ProvServiceTest, WriteVinNotInitialized) {
    std::string vin = "1HGBH41JXMN109186";
    auto result = service_->write_vin(vin);
    EXPECT_EQ(result, ErrorCode::INVALID_STATE);
}

TEST_F(ProvServiceTest, ReadVinSuccess) {
    service_->initialize();
    
    std::string vin = "1HGBH41JXMN109186";
    service_->write_vin(vin);
    
    auto read_vin = service_->read_vin();
    EXPECT_EQ(read_vin, vin);
}

TEST_F(ProvServiceTest, ReadVinNotInitialized) {
    auto read_vin = service_->read_vin();
    EXPECT_EQ(read_vin, "");
}

TEST_F(ProvServiceTest, ReadBindingSuccess) {
    service_->initialize();
    
    std::string vin = "1HGBH41JXMN109186";
    service_->write_vin(vin);
    
    auto binding = service_->read_binding();
    EXPECT_EQ(binding.vin, vin);
    EXPECT_EQ(binding.state, ProvisionState::BOUND);
}

TEST_F(ProvServiceTest, GetProvisionStateSuccess) {
    service_->initialize();
    
    auto state = service_->get_provision_state();
    EXPECT_EQ(state, ProvisionState::NONE);
    
    std::string vin = "1HGBH41JXMN109186";
    service_->write_vin(vin);
    
    state = service_->get_provision_state();
    EXPECT_EQ(state, ProvisionState::BOUND);
}

TEST_F(ProvServiceTest, WriteVehicleConfigWriteProtected) {
    service_->initialize();
    
    std::string vin = "1HGBH41JXMN109186";
    service_->write_vin(vin);
    
    std::vector<uint8_t> config_data = {0x01, 0x02, 0x03, 0x04};
    auto result = service_->write_vehicle_config(config_data);
    EXPECT_EQ(result, ErrorCode::SECURITY_ACCESS_NOT_GRANTED);
}

TEST_F(ProvServiceTest, WriteProductionInfoWriteProtected) {
    service_->initialize();
    
    std::string vin = "1HGBH41JXMN109186";
    service_->write_vin(vin);
    
    ProductionInfo info;
    info.production_date = "2026-06-22";
    info.batch_num = "BATCH001";
    info.station_id = "STATION001";
    
    auto result = service_->write_production_info(info);
    EXPECT_EQ(result, ErrorCode::SECURITY_ACCESS_NOT_GRANTED);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
