cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(sayhisort LANGUAGES CXX)

option(SAYHISORT_ENABLE_TEST "Enable test targets" ON)

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
    find_package(GTest REQUIRED)

    add_executable(
        sayhisort_test
        tests/sayhisort_test.cc
        )
    target_link_libraries(
        sayhisort_test PRIVATE
        sayhisort
        GTest::gtest_main
        )

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

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(sayhisort_test PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()
