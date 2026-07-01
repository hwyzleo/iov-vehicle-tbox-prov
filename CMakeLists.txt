cmake_minimum_required(VERSION 3.10)
project(TboxProvService)

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

# YAML-CPP
find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIRS})

# Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# nlohmann_json
find_package(nlohmann_json REQUIRED)

# Library source files (without main.cpp)
set(LIB_SOURCES
    src/prov_service.cpp
    src/protected_storage.cpp
    src/ecu_uid.cpp
    src/vin_validator.cpp
    src/data_models.cpp
    src/error_codes.cpp
)

# Static library for other modules to link against
add_library(TboxProvLib STATIC ${LIB_SOURCES})
target_include_directories(TboxProvLib PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(TboxProvLib PUBLIC yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json)
target_compile_definitions(TboxProvLib PRIVATE TEST_ENVIRONMENT)

# Main executable
add_executable(TboxProvService src/main.cpp)
target_link_libraries(TboxProvService PRIVATE TboxProvLib)

# Tests
enable_testing()
add_executable(TboxProvTests
    tests/test_prov_service.cpp
    tests/test_protected_storage.cpp
    tests/test_vin_validator.cpp
    tests/test_data_models.cpp
    tests/test_ecu_uid.cpp
)
target_link_libraries(TboxProvTests PRIVATE TboxProvLib GTest::GTest GTest::Main)
target_compile_definitions(TboxProvTests PRIVATE TEST_ENVIRONMENT)
add_test(NAME TboxProvTests COMMAND TboxProvTests)

# CLI tool for testing
add_executable(prov_cli src/prov_cli.cpp)
target_link_libraries(prov_cli PRIVATE TboxProvLib)
target_compile_definitions(prov_cli PRIVATE TEST_ENVIRONMENT)