# TBox PROV - TBOX 产线个性化服务
#
# TBOX-PROV-DSN-CR-009: 接入 TBOX-BUILD 统一 Orin aarch64 构建。
#   - 不设置 CMAKE_C_COMPILER / CMAKE_CXX_COMPILER / CMAKE_SYSROOT（由 BUILD
#     toolchain/preset 注入）
#   - framework 通过已安装 package 消费（find_package + TBoxFramework:: targets）
#   - GNUInstallDirs + 两个显式 install component: prov-sdk / prov-runtime
#   - 无硬编码 RPATH / 宿主路径

cmake_minimum_required(VERSION 3.16)
project(tbox_prov VERSION 0.1.0 LANGUAGES C CXX)

# --- C/C++ 标准（toolchain/preset 可能已设置；不覆盖 Orin preset） ---
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions("-Wall -g")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ===========================================================================
# 依赖：framework SDK + nlohmann_json + GTest(optional)
# ===========================================================================

# --- Framework SDK (config/log/ipc/application) ---
# 本地开发：tbox-env.sh 设置 TBoxFramework_DIR / CMAKE_PREFIX_PATH
# BUILD 交叉编译：orin-aarch64 toolchain 从 TBOX_SDK_STAGING 设置 CMAKE_PREFIX_PATH
# 幂等保护：兄弟项目以 add_subdirectory 引入时跳过重复 find_package
if(NOT TARGET TBoxFramework::config)
    find_package(TboxFramework CONFIG REQUIRED)
endif()

# --- nlohmann_json (PROV 直接使用；BUILD: TBOX_DEP_STAGING；本地: conan) ---
find_package(nlohmann_json REQUIRED)

# --- GTest (仅 HOST 单元测试；BUILD 目标构建不提供) ---
find_package(GTest QUIET)

# ===========================================================================
# Daemon library (TboxProvLib) — 内部库，不安装
# ===========================================================================
set(LIB_SOURCES
    src/prov_service.cpp
    src/protected_storage.cpp
    src/framework_store.cpp
    src/ecu_uid.cpp
    src/se_uid_provider.cpp
    src/tbox_sn_provider.cpp
    src/vin_validator.cpp
    src/data_models.cpp
    src/error_codes.cpp
    src/prov_ipc_dispatcher.cpp
    src/prov_log_adapter.cpp
    src/prov_context.cpp
    src/prov_application.cpp
)

add_library(TboxProvLib STATIC ${LIB_SOURCES})
target_include_directories(TboxProvLib PUBLIC ${PROJECT_SOURCE_DIR}/include)

# TBoxFramework:: targets 携带 include 路径和传递依赖，无需手动 INCLUDE_DIRS
target_link_libraries(TboxProvLib
    PUBLIC
        TBoxFramework::config
        TBoxFramework::log
        TBoxFramework::ipc
        TBoxFramework::application
        nlohmann_json::nlohmann_json
)
target_compile_definitions(TboxProvLib PRIVATE TEST_ENVIRONMENT)

# ===========================================================================
# Client SDK library (tbox_prov_client / tbox::prov_client)
# ===========================================================================
set(CLIENT_SOURCES
    src/prov_client.cpp
    src/error_codes.cpp
)

add_library(tbox_prov_client STATIC ${CLIENT_SOURCES})
add_library(tbox::prov_client ALIAS tbox_prov_client)
set_target_properties(tbox_prov_client PROPERTIES EXPORT_NAME prov_client)

# PUBLIC: only business facade/DTO headers (no framework IPC leakage to callers)
target_include_directories(tbox_prov_client
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# PRIVATE: framework-ipc (传输细节不泄漏给调用方) + framework-log
# (prov_client 直接使用 Logger::get；TBOX-PROV-DSN-CR-009 修复：
#  SDK 不依赖 daemon 内部 ProvLogAdapter) + nlohmann_json (公开头文件未使用)
target_link_libraries(tbox_prov_client
    PRIVATE
        TBoxFramework::ipc
        TBoxFramework::log
        nlohmann_json::nlohmann_json
)
target_compile_definitions(tbox_prov_client PRIVATE TEST_ENVIRONMENT)

# ===========================================================================
# Main executable (daemon)
# ===========================================================================
# TBOX-PROV-DSN-CR-008 §10.1: 构建期切换 Application 生命周期。
#   ON  -> ProvApplication（hwyz::Application 编排）
#   OFF -> 旧式手写生命周期（回滚验证用，验收后删除）
option(PROV_USE_FRAMEWORK_APPLICATION "Use hwyz::Application lifecycle" ON)
if(PROV_USE_FRAMEWORK_APPLICATION)
    add_executable(tbox_prov src/main.cpp)
else()
    add_executable(tbox_prov src/main_legacy.cpp)
endif()
target_link_libraries(tbox_prov PRIVATE TboxProvLib)

# ===========================================================================
# Install rules — 两个显式 component (CR-009 §5.3 / BD6)
#
#   prov-sdk     -> staging: sdk      (公开头文件 / 静态库 / CMake package)
#   prov-runtime -> staging: rootfs   (daemon / systemd unit / 配置模板)
# ===========================================================================

# --- prov-runtime: daemon binary ---
install(TARGETS tbox_prov
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT prov-runtime
)

# --- prov-runtime: systemd unit ---
install(FILES packaging/systemd/tbox-prov.service
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system
    COMPONENT prov-runtime
)

# --- prov-runtime: default config (production-safe, no test uid/sn) ---
# TBOX-PROV-DSN-CR-010 §4: 仅安装 prov.default.yaml -> /etc/tbox/conf.d/prov.yaml。
#   正式 /etc/tbox/common.yaml 不再由 PROV 安装（由 BUILD 唯一提供）。
#   config/dev/** 调试文件不安装（禁止 install(DIRECTORY config/ ...) 递归规则）。
install(FILES config/prov.default.yaml
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/tbox/conf.d
    RENAME prov.yaml
    COMPONENT prov-runtime
)

# --- prov-sdk: public headers ---
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/tbox/prov/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tbox/prov
    COMPONENT prov-sdk
    FILES_MATCHING PATTERN "*.h"
)

# --- prov-sdk: client static library + export ---
install(TARGETS tbox_prov_client
    EXPORT TboxProvClientTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT prov-sdk
)

install(EXPORT TboxProvClientTargets
    FILE TboxProvClientTargets.cmake
    NAMESPACE tbox::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxProvClient
    COMPONENT prov-sdk
)

# --- prov-sdk: CMake package config (relocatable) ---
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TboxProvClientConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/TboxProvClientConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxProvClient
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/TboxProvClientConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/TboxProvClientConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/TboxProvClientConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxProvClient
    COMPONENT prov-sdk
)

# ===========================================================================
# Tests (HOST only; BUILD 目标构建仅构建 declared targets，不编译测试)
# ===========================================================================
if(GTest_FOUND)
    enable_testing()

    add_executable(TboxProvTests
        tests/test_prov_service.cpp
        tests/test_protected_storage.cpp
        tests/test_vin_validator.cpp
        tests/test_data_models.cpp
        tests/test_ecu_uid.cpp
        tests/test_tbox_sn_provider.cpp
        tests/test_ipc.cpp
        tests/test_log_adapter.cpp
        tests/test_context.cpp
        tests/test_log_events.cpp
        tests/test_log_integration.cpp
        tests/test_prov_ipc_dispatcher.cpp
        tests/test_prov_application.cpp
        tests/test_config_assets.cpp
    )
    target_link_libraries(TboxProvTests PRIVATE TboxProvLib tbox_prov_client GTest::GTest GTest::Main)
    target_compile_definitions(TboxProvTests PRIVATE TEST_ENVIRONMENT)
    add_test(NAME TboxProvTests COMMAND TboxProvTests)

    # CLI tool for testing (not installed)
    add_executable(prov_cli src/prov_cli.cpp)
    target_link_libraries(prov_cli PRIVATE TboxProvLib)
    target_compile_definitions(prov_cli PRIVATE TEST_ENVIRONMENT)
endif()
