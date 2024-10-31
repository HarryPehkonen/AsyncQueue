cmake_minimum_required(VERSION 3.14)
project(async_queue VERSION 0.1.0 LANGUAGES CXX)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add option to build tests and examples
option(ASYNC_QUEUE_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(ASYNC_QUEUE_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})

# Create interface library for the header-only library
add_library(async_queue INTERFACE)
add_library(async_queue::async_queue ALIAS async_queue)

# Set up include directories
target_include_directories(async_queue INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Examples
if(ASYNC_QUEUE_BUILD_EXAMPLES)
    add_executable(producer_consumer examples/producer_consumer.cpp)
    target_link_libraries(producer_consumer PRIVATE 
        async_queue 
        pthread
    )
endif()

# Tests configuration (unchanged)
if(ASYNC_QUEUE_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_executable(async_queue_tests tests/basic_tests.cpp)
    target_link_libraries(async_queue_tests 
        PRIVATE 
        async_queue
        GTest::GTest 
        GTest::Main
        pthread
    )
    include(GoogleTest)
    gtest_discover_tests(async_queue_tests)
endif()

# Installation rules
install(TARGETS async_queue
    EXPORT async_queue-targets
    INCLUDES DESTINATION include
)

install(FILES include/async_queue/async_queue.hpp
    DESTINATION include/async_queue
)

install(EXPORT async_queue-targets
    FILE async_queue-targets.cmake
    NAMESPACE async_queue::
    DESTINATION lib/cmake/async_queue
)

# Package version file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/async_queue-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install package config files
install(FILES
    "${PROJECT_SOURCE_DIR}/cmake/async_queue-config.cmake"
    "${PROJECT_BINARY_DIR}/async_queue-config-version.cmake"
    DESTINATION lib/cmake/async_queue
)
