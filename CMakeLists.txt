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

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(sayhisort_test PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(sayhisort_cpp20_test PRIVATE -std=c++20 -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()
