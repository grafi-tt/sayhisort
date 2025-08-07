cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(sayhisort LANGUAGES CXX)

option(SAYHISORT_ENABLE_TEST "Enable test targets" ON)
option(SAYHISORT_USE_SYSTEM_GTEST "Use system GTest" OFF)

add_library(sayhisort INTERFACE sayhisort.h)
install(
    TARGETS sayhisort
    EXPORT sayhisort-config
    INCLUDES DESTINATION include
    )
install(
    FILES sayhisort.h
    DESTINATION include
    )

target_include_directories(
    sayhisort INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )
target_compile_features(
    sayhisort INTERFACE
    cxx_std_17
    )

if(SAYHISORT_ENABLE_TEST)
    enable_testing()
    if(NOT SAYHISORT_USE_SYSTEM_GTEST)
        include(FetchContent)
        FetchContent_Declare(
          googletest
          URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
          URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    else()
        find_package(GTest REQUIRED)
    endif()

    add_executable(
        sayhisort_test
        tests/sayhisort_test.cc
        tests/sayhisort_test_util.cc
        tests/sayhisort_test_util.h
        )
    target_link_libraries(
        sayhisort_test PRIVATE
        sayhisort
        GTest::gtest_main
        )
    add_test(NAME sayhisort_test COMMAND $<TARGET_FILE:sayhisort_test>)

    add_executable(
        sayhisort_cpp20_test
        tests/sayhisort_cpp20_test.cc
        )
    target_link_libraries(
        sayhisort_cpp20_test PRIVATE
        sayhisort
        )
    target_compile_features(
        sayhisort_cpp20_test PRIVATE
        cxx_std_20
        )
    add_test(NAME sayhisort_cpp20_test COMMAND $<TARGET_FILE:sayhisort_cpp20_test>)

    add_executable(
        sayhisort_bench
        tests/sayhisort_bench.cc
        tests/sayhisort_bench_data.cc
        tests/sayhisort_bench_data.h
        tests/sayhisort_profile_util.cc
        tests/sayhisort_profile_util.h
        tests/sayhisort_test_util.cc
        tests/sayhisort_test_util.h
        )
    target_link_libraries(
        sayhisort_bench PRIVATE
        sayhisort
        )
    target_compile_features(
        sayhisort_bench PRIVATE
        cxx_std_23
        )
    add_test(NAME sayhisort_bench COMMAND $<TARGET_FILE:sayhisort_bench>)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(sayhisort_test PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror)
        target_compile_options(sayhisort_cpp20_test PRIVATE -std=c++20 -Wall -Wextra -Wpedantic -Werror)
        target_compile_options(sayhisort_bench PRIVATE -std=c++23 -Wall -Wextra -Wpedantic -Werror -O3 -march=native)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(sayhisort_bench PRIVATE /Zc:preprocessor)
    endif()
endif()
