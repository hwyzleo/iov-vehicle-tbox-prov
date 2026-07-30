#include <gtest/gtest.h>
#include "protected_storage_impl.h"
#include "framework_store.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cstdio>
#include <ctime>

namespace tbox {
namespace prov {
namespace testing {

class ProtectedStorageTest : public ::testing::Test {
protected:
    std::string test_dir_ = "/tmp/prov_test_storage";
    std::unique_ptr<tbox::framework::Store> store_;
    
    void SetUp() override {
        // 清理测试目录
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
        store_ = std::make_unique<tbox::framework::Store>("prov", test_dir_);
    }
    
    void TearDown() override {
        store_.reset();
        // 清理测试目录
        std::filesystem::remove_all(test_dir_);
    }
};

TEST_F(ProtectedStorageTest, Initialize) {
    ProtectedStorageImpl storage(*store_);
    EXPECT_EQ(storage.initialize(), ErrorCode::SUCCESS);
}

TEST_F(ProtectedStorageTest, ReadWriteVehicleBinding) {
    ProtectedStorageImpl storage(*store_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);
    
    // 创建测试数据
    VehicleBinding binding;
    binding.vin = "1HGBH41JXMN109186";
    binding.ecu_uid = "ECU123456789";
    binding.sn = "TBOX-SN-12345";
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
    EXPECT_EQ(result->sn, binding.sn);
    EXPECT_EQ(result->state, binding.state);
    EXPECT_EQ(result->locked, binding.locked);
    EXPECT_EQ(result->retry_count, binding.retry_count);
    EXPECT_EQ(result->rewrite_count, binding.rewrite_count);
}

TEST_F(ProtectedStorageTest, ReadWriteVehicleConfig) {
    ProtectedStorageImpl storage(*store_);
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
    ProtectedStorageImpl storage(*store_);
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
    ProtectedStorageImpl storage(*store_);
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
    ProtectedStorageImpl storage(*store_);
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
    ProtectedStorageImpl storage(*store_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);
    
    // 读取不存在的数据
    auto binding = storage.read_vehicle_binding();
    EXPECT_FALSE(binding.has_value());
    
    auto config = storage.read_vehicle_config();
    EXPECT_FALSE(config.has_value());
    
    auto info = storage.read_production_info();
    EXPECT_FALSE(info.has_value());
}

// ============================================================
// CR-007: 旧持久化快照兼容（无 sn 字段）
// ============================================================
TEST_F(ProtectedStorageTest, OldSnapshotWithoutSnLoadsAsEmpty) {
    // 旧版本持久化的绑定快照不含 sn 字段时，反序列化 sn 为空，其余字段保持不变；
    // 不得以 ecu_uid 作为缺失 sn 的默认值。
    ProtectedStorageImpl storage(*store_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);

    // 模拟旧版本快照（无 sn 字段）
    nlohmann::json old_snapshot;
    old_snapshot["vin"] = "1HGBH41JXMN109186";
    old_snapshot["ecu_uid"] = "ECU123456789";
    old_snapshot["state"] = static_cast<uint8_t>(ProvisionState::BOUND);
    old_snapshot["locked"] = true;
    old_snapshot["bound_at"] = static_cast<std::time_t>(0);
    old_snapshot["last_error"] = "";
    old_snapshot["retry_count"] = 0;
    old_snapshot["rewrite_count"] = 0;
    old_snapshot["last_rewrite_at"] = static_cast<std::time_t>(0);
    // 故意不写入 sn 字段

    ASSERT_TRUE(store_->save<std::string>("binding", old_snapshot.dump()));

    // 读取：应加载成功，sn 为空，其他字段保持
    auto result = storage.read_vehicle_binding();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->vin, "1HGBH41JXMN109186");
    EXPECT_EQ(result->ecu_uid, "ECU123456789");
    EXPECT_EQ(result->sn, "");  // 旧快照无 sn -> 空，由 Provider 补齐
    EXPECT_NE(result->sn, result->ecu_uid);  // 不得以 ecu_uid 代填
    EXPECT_EQ(result->state, ProvisionState::BOUND);
    EXPECT_TRUE(result->locked);
    EXPECT_EQ(result->retry_count, 0);
    EXPECT_EQ(result->rewrite_count, 0);
}

TEST_F(ProtectedStorageTest, NewSnapshotWithSnRoundTrip) {
    // 新版本快照含 sn 字段时，读写往返保持 sn 值
    ProtectedStorageImpl storage(*store_);
    ASSERT_EQ(storage.initialize(), ErrorCode::SUCCESS);

    VehicleBinding binding;
    binding.vin = "1HGBH41JXMN109186";
    binding.ecu_uid = "ECU123456789";
    binding.sn = "TBOX-SN-99999";
    binding.state = ProvisionState::BOUND;
    binding.locked = false;

    ASSERT_EQ(storage.write_vehicle_binding(binding), ErrorCode::SUCCESS);

    auto result = storage.read_vehicle_binding();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->sn, "TBOX-SN-99999");
    EXPECT_EQ(result->vin, binding.vin);
    EXPECT_EQ(result->ecu_uid, binding.ecu_uid);
}

} // namespace testing
} // namespace prov
} // namespace tbox