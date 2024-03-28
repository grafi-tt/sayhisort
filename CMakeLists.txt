cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
project(sayhisort LANGUAGES CXX)

option(SAYHISORT_ENABLE_TEST "Enable test targets" ON)

if(SAYHISORT_ENABLE_TEST)
    enable_testing()
    find_package(GTest REQUIRED)

    add_executable(
        sayhisort_test
        sayhisort_test.cc
        )
    target_link_libraries(
        sayhisort_test
        GTest::gtest_main
        )

    target_compile_features(sayhisort_test PRIVATE cxx_std_17)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(sayhisort_test PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()
