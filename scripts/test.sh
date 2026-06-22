#!/bin/bash

# TBOX PROV Service Test Script

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 运行单元测试
run_unit_tests() {
    print_info "Running unit tests..."
    
    cd build
    
    # 运行所有测试
    ctest --output-on-failure --verbose
    
    cd ..
}

# 运行特定测试
run_specific_test() {
    local test_name=$1
    
    print_info "Running specific test: $test_name"
    
    cd build
    
    # 运行特定测试
    ctest -R "$test_name" --output-on-failure --verbose
    
    cd ..
}

# 生成测试覆盖率
generate_coverage() {
    print_info "Generating test coverage..."
    
    # 检查lcov是否安装
    if ! command -v lcov &> /dev/null; then
        print_error "lcov is not installed"
        exit 1
    fi
    
    cd build
    
    # 清除之前的覆盖率数据
    lcov --directory . --zerocounters
    
    # 运行测试
    ctest --output-on-failure
    
    # 收集覆盖率数据
    lcov --directory . --capture --output-file coverage.info
    
    # 过滤系统文件
    lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
    
    # 生成HTML报告
    genhtml coverage.info --output-directory coverage_report
    
    cd ..
    
    print_info "Coverage report generated in build/coverage_report/index.html"
}

# 显示帮助
show_help() {
    echo "TBOX PROV Service Test Script"
    echo ""
    echo "Usage: $0 [OPTION] [TEST_NAME]"
    echo ""
    echo "Options:"
    echo "  all        Run all unit tests"
    echo "  specific   Run specific test (requires TEST_NAME)"
    echo "  coverage   Generate test coverage report"
    echo "  help       Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 all                    # Run all tests"
    echo "  $0 specific VinValidator  # Run VinValidator tests"
    echo "  $0 coverage               # Generate coverage report"
}

# 主函数
main() {
    case "${1:-all}" in
        all)
            run_unit_tests
            ;;
        specific)
            if [ -z "$2" ]; then
                print_error "Test name is required for 'specific' option"
                show_help
                exit 1
            fi
            run_specific_test "$2"
            ;;
        coverage)
            generate_coverage
            ;;
        help)
            show_help
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
    
    print_info "Tests completed successfully"
}

# 执行主函数
main "$@"