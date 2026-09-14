cmake_minimum_required(VERSION 3.10.0)
project(esp_ebus_app_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project root is one level up from test_host/
set(PROJECT_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# Add mocks BEFORE app includes
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/freertos
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/sys
    ${PROJECT_ROOT}/include
    ${PROJECT_ROOT}/lib/ebus/include
)

# Use mock Logger instead of real one
set(APP_SOURCES
    ${PROJECT_ROOT}/src/app/command.cpp
    ${PROJECT_ROOT}/src/app/command_manager.cpp
    ${PROJECT_ROOT}/src/config/app_config.cpp
    ${PROJECT_ROOT}/src/config/app_config_loader.cpp
    ${PROJECT_ROOT}/src/config/config_manager.cpp
    ${PROJECT_ROOT}/src/app/cron.cpp
    ${PROJECT_ROOT}/src/app/ebus_accessor.cpp
    ${PROJECT_ROOT}/src/app/data_profile.cpp
    ${PROJECT_ROOT}/src/app/ha_profile.cpp
    ${PROJECT_ROOT}/src/app/mqtt_ha.cpp
    ${PROJECT_ROOT}/src/system/string_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mqtt_mock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/nvs_mock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/http_stubs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/app_stub.cpp
)

# Test files (explicit: adding a test file requires listing it here).
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_config_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/config_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cron_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/data_profile_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ha_profile_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string_pool_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wifi_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mqtt_ha_tests.cpp
)

# Create test executable
add_executable(app_tests ${APP_SOURCES} ${TEST_SOURCES})

# Disable ebus library internal features not needed for tests
target_compile_definitions(app_tests PRIVATE
    EBUS_INTERNAL=1
    EBUS_SIMULATION=0
    ESP32=1
    AUTO_VERSION="host-test"
)

# Find and link Catch2 from the ebus library build
find_package(Catch2 REQUIRED PATHS ${PROJECT_ROOT}/lib/ebus/build/_deps/catch2-src)
target_link_libraries(app_tests PRIVATE
    ${PROJECT_ROOT}/lib/ebus/build/src/ebus/libebus.a
    Catch2::Catch2WithMain
)

enable_testing()
include(CTest)
