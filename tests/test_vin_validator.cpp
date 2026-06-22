#include <gtest/gtest.h>
#include "vin_validator.h"

namespace tbox {
namespace prov {
namespace testing {

class VinValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(VinValidatorTest, ValidVin) {
    // 测试有效的VIN
    EXPECT_TRUE(VinValidator::validate("1HGBH41JXMN109186"));
    EXPECT_TRUE(VinValidator::validate("11111111111111111"));
}

TEST_F(VinValidatorTest, InvalidLength) {
    // 测试无效长度
    EXPECT_FALSE(VinValidator::validate("1HGBH41JXMN10918"));  // 16位
    EXPECT_FALSE(VinValidator::validate("1HGBH41JXMN1091861")); // 18位
    EXPECT_FALSE(VinValidator::validate(""));                   // 空
}

TEST_F(VinValidatorTest, InvalidCharset) {
    // 测试非法字符（I、O、Q）
    EXPECT_FALSE(VinValidator::validate("1HGBH41JXMN1091I6"));  // 包含I
    EXPECT_FALSE(VinValidator::validate("1HGBH41JXMN1091O6"));  // 包含O
    EXPECT_FALSE(VinValidator::validate("1HGBH41JXMN1091Q6"));  // 包含Q
}

TEST_F(VinValidatorTest, InvalidCheckDigit) {
    // 测试无效校验位
    EXPECT_FALSE(VinValidator::validate("1HGBH41JXMN109187"));  // 校验位错误
}

TEST_F(VinValidatorTest, ValidationErrorMessages) {
    // 测试错误信息
    EXPECT_EQ(VinValidator::get_validation_error("1HGBH41JXMN10918"), "VIN长度必须为17位");
    EXPECT_EQ(VinValidator::get_validation_error("1HGBH41JXMN1091I6"), "VIN包含非法字符（I、O、Q不允许）");
    EXPECT_EQ(VinValidator::get_validation_error("1HGBH41JXMN109187"), "VIN校验位不正确");
    EXPECT_EQ(VinValidator::get_validation_error("1HGBH41JXMN109186"), "");
}

} // namespace testing
} // namespace prov
} // namespace tbox