#!/bin/bash
# 验证错误码映射
cd /Users/hwyz_leo/Projects/open-iov/vehicle/tbox/iov-vehicle-tbox-prov/build

echo "=== 验证错误码映射 ==="
echo ""

# 编译并运行错误码验证程序
cat > /tmp/verify_error_codes.cpp << 'EOF'
#include "error_codes.h"
#include <iostream>
#include <map>

using namespace tbox::prov;

int main() {
    std::map<ErrorCode, std::string> expected_mappings = {
        {ErrorCode::SUCCESS, "SUCCESS"},
        {ErrorCode::SECURITY_ACCESS_NOT_GRANTED, "PROV-1001"},
        {ErrorCode::INVALID_VIN_FORMAT, "PROV-1002"},
        {ErrorCode::VIN_WRITE_FAILED, "PROV-1003"},
        {ErrorCode::READBACK_VERIFICATION_FAILED, "PROV-1004"},
        {ErrorCode::VIN_CONFLICT_UNAUTHORIZED, "PROV-1005"},
        {ErrorCode::CONFIG_WRITE_FAILED, "PROV-1006"},
        {ErrorCode::PRODUCTION_INFO_WRITE_FAILED, "PROV-1007"},
        {ErrorCode::SE_UID_READ_FAILED, "PROV-1008"},
        {ErrorCode::SE_MISSING_CONFIG_NOT_FOUND, "PROV-1009"},
        {ErrorCode::SE_MISSING_PRODUCTION_FAIL_CLOSED, "PROV-1010"},
        {ErrorCode::INTERNAL_ERROR, "INTERNAL-9999"},
        {ErrorCode::STORAGE_ERROR, "INTERNAL-9998"},
        {ErrorCode::ECU_UID_READ_ERROR, "INTERNAL-9997"},
        {ErrorCode::INVALID_STATE, "INTERNAL-9996"}
    };

    int passed = 0;
    int failed = 0;

    for (const auto& [code, expected] : expected_mappings) {
        std::string actual = error_code_to_string(code);
        if (actual == expected) {
            std::cout << "✓ " << expected << " -> " << actual << std::endl;
            passed++;
        } else {
            std::cout << "✗ " << expected << " -> " << actual << " (expected: " << expected << ")" << std::endl;
            failed++;
        }
    }

    std::cout << std::endl;
    std::cout << "结果: " << passed << " 通过, " << failed << " 失败" << std::endl;

    return failed > 0 ? 1 : 0;
}
EOF

# 编译验证程序
g++ -std=c++17 -I../include -L. -lTboxProvLib -o /tmp/verify_error_codes /tmp/verify_error_codes.cpp

if [ $? -ne 0 ]; then
    echo "编译失败"
    exit 1
fi

# 运行验证程序
/tmp/verify_error_codes

if [ $? -eq 0 ]; then
    echo ""
    echo "错误码映射验证通过！"
else
    echo ""
    echo "错误码映射验证失败！"
    exit 1
fi