cmake_minimum_required(VERSION 3.10)
project(tbox_prov)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions("-Wall -lpthread -g")

include(GNUInstallDirs)

# Platform-specific library directories
if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    if (APPLE)
        set(LIB_DIR ${PROJECT_SOURCE_DIR}/third_party/lib/arm64-apple-darwin)
    elseif (UNIX AND NOT APPLE)
        set(LIB_DIR ${PROJECT_SOURCE_DIR}/third_party/lib/aarch64-linux-gnu)
    endif ()
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    if (APPLE)
        set(LIB_DIR ${PROJECT_SOURCE_DIR}/third_party/lib/x86_64-apple-darwin)
    elseif (UNIX AND NOT APPLE)
        set(LIB_DIR ${PROJECT_SOURCE_DIR}/third_party/lib/x86_64-linux-gnu)
    endif ()
endif ()

# TBox Framework：统一通过 find_package 消费（自包含 yaml-cpp / spdlog / config / store / ipc）
# 查找前缀由各项目 scripts/build.sh 从 iov-vehicle-tbox-framework/scripts/tbox-env.sh
# 取得的 TBOX_PREFIX 决定，切换本地/生产环境无需改动本文件。
# 本项目也可能被兄弟项目（如 diag）以 add_subdirectory 方式引入，故做幂等保护。
if(NOT TARGET TBoxFramework::tbox-framework)
    find_package(TBoxFramework REQUIRED)
endif()

# Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# nlohmann_json
find_package(nlohmann_json REQUIRED)

# ============================================================
# Daemon library (TboxProvLib)
# ============================================================
set(LIB_SOURCES
    src/prov_service.cpp
    src/protected_storage.cpp
    src/framework_store.cpp
    src/ecu_uid.cpp
    src/vin_validator.cpp
    src/data_models.cpp
    src/error_codes.cpp
    src/prov_ipc_dispatcher.cpp
    src/prov_log_adapter.cpp
    src/prov_context.cpp
)

add_library(TboxProvLib STATIC ${LIB_SOURCES})
target_include_directories(TboxProvLib PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(TboxProvLib PUBLIC ${TBoxFramework_LIBRARIES} nlohmann_json::nlohmann_json)
target_include_directories(TboxProvLib PUBLIC ${TBoxFramework_INCLUDE_DIRS})
target_compile_definitions(TboxProvLib PRIVATE TEST_ENVIRONMENT)

# ============================================================
# Client SDK library (tbox_prov_client / tbox::prov_client)
# ============================================================
set(CLIENT_SOURCES
    src/prov_client.cpp
    src/error_codes.cpp
)

add_library(tbox_prov_client STATIC ${CLIENT_SOURCES})
add_library(tbox::prov_client ALIAS tbox_prov_client)

# PUBLIC: only business facade/DTO headers
target_include_directories(tbox_prov_client
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# PRIVATE: framework-ipc link and internal headers
target_link_libraries(tbox_prov_client
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE ${TBoxFramework_LIBRARIES}
)
target_include_directories(tbox_prov_client PRIVATE ${TBoxFramework_INCLUDE_DIRS})
target_compile_definitions(tbox_prov_client PRIVATE TEST_ENVIRONMENT)

# Install public SDK headers
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/tbox/prov/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tbox/prov
    FILES_MATCHING PATTERN "*.h"
)

# Install client library
install(TARGETS tbox_prov_client
    EXPORT TboxProvClientTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Export CMake config
install(EXPORT TboxProvClientTargets
    FILE TboxProvClientTargets.cmake
    NAMESPACE tbox::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxProvClient
)

# CMake config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TboxProvClientConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/TboxProvClientConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxProvClient
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/TboxProvClientConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxProvClient
)

set(LIB_INSTALL_DIR "../lib" CACHE PATH "Relative path to library directory")
get_filename_component(LIB_INSTALL_DIR_ABS "${LIB_INSTALL_DIR}" ABSOLUTE)
set(CMAKE_INSTALL_RPATH "${LIB_INSTALL_DIR_ABS}")

# ============================================================
# Main executable
# ============================================================
add_executable(tbox_prov src/main.cpp)
target_link_libraries(tbox_prov PRIVATE TboxProvLib)

# ============================================================
# Tests
# ============================================================
enable_testing()
add_executable(TboxProvTests
    tests/test_prov_service.cpp
    tests/test_protected_storage.cpp
    tests/test_vin_validator.cpp
    tests/test_data_models.cpp
    tests/test_ecu_uid.cpp
    tests/test_ipc.cpp
    tests/test_log_adapter.cpp
    tests/test_context.cpp
    tests/test_log_events.cpp
    tests/test_log_integration.cpp
    tests/test_prov_ipc_dispatcher.cpp
)
target_link_libraries(TboxProvTests PRIVATE TboxProvLib tbox_prov_client GTest::GTest GTest::Main)
target_compile_definitions(TboxProvTests PRIVATE TEST_ENVIRONMENT)
add_test(NAME TboxProvTests COMMAND TboxProvTests)

# CLI tool for testing
add_executable(prov_cli src/prov_cli.cpp)
target_link_libraries(prov_cli PRIVATE TboxProvLib)
target_compile_definitions(prov_cli PRIVATE TEST_ENVIRONMENT)
