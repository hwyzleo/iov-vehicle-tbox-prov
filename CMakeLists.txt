cmake_minimum_required(VERSION 3.10)
project(tbox_prov)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions("-Wall -lpthread -g")

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

# TBox Framework (provides yaml-cpp, spdlog, config management)
find_package(TBoxFramework REQUIRED)

# Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# nlohmann_json
find_package(nlohmann_json REQUIRED)

# Library source files (without main.cpp)
set(LIB_SOURCES
    src/prov_service.cpp
    src/protected_storage.cpp
    src/framework_store.cpp
    src/ecu_uid.cpp
    src/vin_validator.cpp
    src/data_models.cpp
    src/error_codes.cpp
    src/ipc_protocol.cpp
    src/ipc_server.cpp
    src/prov_log_adapter.cpp
    src/prov_context.cpp
)

# Static library for other modules to link against
add_library(TboxProvLib STATIC ${LIB_SOURCES})
target_include_directories(TboxProvLib PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(TboxProvLib PUBLIC ${TBoxFramework_LIBRARIES} nlohmann_json::nlohmann_json)
target_include_directories(TboxProvLib PUBLIC ${TBoxFramework_INCLUDE_DIR})
target_compile_definitions(TboxProvLib PRIVATE TEST_ENVIRONMENT)

# Client stub library for IPC communication
set(CLIENT_SOURCES
    src/prov_client.cpp
    src/ipc_protocol.cpp
    src/error_codes.cpp
)
add_library(ProvClient STATIC ${CLIENT_SOURCES})
target_include_directories(ProvClient PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ProvClient PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(ProvClient PRIVATE TEST_ENVIRONMENT)

set(LIB_INSTALL_DIR "../lib" CACHE PATH "Relative path to library directory")
get_filename_component(LIB_INSTALL_DIR_ABS "${LIB_INSTALL_DIR}" ABSOLUTE)
set(CMAKE_INSTALL_RPATH "${LIB_INSTALL_DIR_ABS}")
# 不要使用 CMAKE_BUILD_WITH_INSTALL_RPATH，它会覆盖 CMake 自动计算的 build RPATH
# 导致 conan 依赖库（yaml-cpp、gtest）找不到
# CMake 会根据链接目标自动设置 build RPATH，包含所有必要的库目录

# Main executable
add_executable(tbox_prov src/main.cpp)
target_link_libraries(tbox_prov PRIVATE TboxProvLib)

# Tests
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
)
target_link_libraries(TboxProvTests PRIVATE TboxProvLib ProvClient GTest::GTest GTest::Main)
target_compile_definitions(TboxProvTests PRIVATE TEST_ENVIRONMENT)
add_test(NAME TboxProvTests COMMAND TboxProvTests)

# CLI tool for testing
add_executable(prov_cli src/prov_cli.cpp)
target_link_libraries(prov_cli PRIVATE TboxProvLib)
target_compile_definitions(prov_cli PRIVATE TEST_ENVIRONMENT)