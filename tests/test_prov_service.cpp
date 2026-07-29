#include <gtest/gtest.h>
#include "prov_service.h"
#include "framework_store.h"
#include "config.h"
#include <filesystem>
#include <chrono>
#include <fstream>

using namespace tbox::prov;

class ProvServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/prov_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        // 创建测试配置目录结构（ConfigManager 需要 common.yaml 和 conf.d/prov.yaml）
        std::filesystem::path config_dir = test_dir_ + "/conf.d";
        std::filesystem::create_directories(config_dir);
        
        // 创建 common.yaml（必需，框架校验器要求 common.log 配置）
        std::filesystem::path common_config = test_dir_ + "/common.yaml";
        {
            std::ofstream file(common_config);
            file << "# TBOX Common Configuration\n";
            file << "common:\n";
            file << "  log:\n";
            file << "    type: console\n";
            file << "    path: ./log.txt\n";
            file.close();
        }
        
        // 复制配置文件到测试目录
        std::filesystem::path src_config = std::filesystem::current_path().parent_path() / "config" / "prov.yaml";
        std::filesystem::path dst_config = config_dir / "prov.yaml";
        if (std::filesystem::exists(src_config)) {
            std::filesystem::copy_file(src_config, dst_config, std::filesystem::copy_options::overwrite_existing);
        } else {
            // 如果找不到配置文件，创建一个基本的配置
            std::ofstream file(dst_config);
            file << "ecu:\n";
            file << "  uid: \"00000000000000000000000000000001\"\n";
            file.close();
        }
        
        // 加载框架配置（使用测试目录作为配置根目录）
        CONFIG_MANAGER.load("prov", test_dir_);
        
        store_ = std::make_unique<tbox::framework::Store>("prov", test_dir_);
        config_.enable_write_protection = true;
        config_.max_retry_count = 3;
        
        service_ = std::make_unique<ProvService>(*store_, config_);
    }
    
    void TearDown() override {
        service_.reset();
        store_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::string test_dir_;
    std::unique_ptr<tbox::framework::Store> store_;
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
