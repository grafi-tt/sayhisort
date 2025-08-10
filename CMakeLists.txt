cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(sayhisort LANGUAGES C CXX)

option(SAYHISORT_ENABLE_TEST "Enable test targets" ON)
option(SAYHISORT_USE_SYSTEM_GTEST "Use system GTest" OFF)
option(SAYHISORT_THIRDPARTY_BENCH "Build benchmark with third party libs (take effect if SAYHISORT_ENABLE_TEST is ON)" OFF)

add_library(sayhisort INTERFACE sayhisort.h)
install(
    TARGETS sayhisort
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
    set(SAYHISORT_CONTENT_NAMES)

    if(NOT SAYHISORT_USE_SYSTEM_GTEST)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
            URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
            EXCLUDE_FROM_ALL
            )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(SAYHISORT_CONTENT_NAMES ${SAYHISORT_CONTENT_NAMES} googletest)
    else()
        find_package(GTest REQUIRED)
    endif()

    if(SAYHISORT_THIRDPARTY_BENCH)
        FetchContent_Declare(
            wikisort
            GIT_REPOSITORY https://github.com/BonzaiThePenguin/WikiSort.git
            GIT_TAG df036bd85af713f5012ccc4d329f05eb6a553128
            PATCH_COMMAND git checkout . && git apply ${CMAKE_CURRENT_SOURCE_DIR}/tests/wikisort.patch
            )
        set(SAYHISORT_CONTENT_NAMES ${SAYHISORT_CONTENT_NAMES} wikisort)
        FetchContent_Declare(
            octosort
            GIT_REPOSITORY https://github.com/scandum/octosort.git
            GIT_TAG 73605cdbdfec66e7112c6a3a3830748fcd3bb665
            )
        set(SAYHISORT_CONTENT_NAMES ${SAYHISORT_CONTENT_NAMES} octosort)
        FetchContent_Declare(
            logsort
            GIT_REPOSITORY https://github.com/aphitorite/Logsort.git
            GIT_TAG 96d3fcfd2e32797d4aa99d61f11a650ce47dac5a
            )
        set(SAYHISORT_CONTENT_NAMES ${SAYHISORT_CONTENT_NAMES} logsort)
    endif()

    FetchContent_MakeAvailable(${SAYHISORT_CONTENT_NAMES})

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
        tests/sayhisort_profile_util.h
        tests/sayhisort_test_util.cc
        tests/sayhisort_test_util.h
        )
    target_compile_features(
        sayhisort_bench PRIVATE
        cxx_std_23
        )
    set(SAYHISORT_BENCH_LIBS sayhisort)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(sayhisort_test PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror)
        target_compile_options(sayhisort_cpp20_test PRIVATE -std=c++20 -Wall -Wextra -Wpedantic -Werror)
        target_compile_options(sayhisort_bench PRIVATE -std=c++23 -Wall -Wextra -Wpedantic -Werror -O3 -march=native)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(sayhisort_bench PRIVATE /Zc:preprocessor)
    endif()

    if(SAYHISORT_THIRDPARTY_BENCH)
        target_compile_definitions(sayhisort_bench PRIVATE SAYHISORT_THIRDPARTY_BENCH=1)
        add_library(
            sayhisort_bench_wikisort STATIC
            tests/third_party/wikisort_bench.cc
            tests/third_party/wikisort_bench.h
            )
        target_include_directories(sayhisort_bench_wikisort PRIVATE ${FETCHCONTENT_BASE_DIR}/wikisort-src)
        set(SAYHISORT_BENCH_LIBS ${SAYHISORT_BENCH_LIBS} sayhisort_bench_wikisort)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(sayhisort_bench_wikisort PRIVATE -O3 -march=native)
        endif()

        add_library(
            sayhisort_bench_octosort STATIC
            tests/third_party/octosort_bench.c
            tests/third_party/octosort_bench.h
            )
        target_include_directories(sayhisort_bench_octosort PRIVATE ${FETCHCONTENT_BASE_DIR}/octosort-src)
        set(SAYHISORT_BENCH_LIBS ${SAYHISORT_BENCH_LIBS} sayhisort_bench_octosort)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
            target_compile_options(sayhisort_bench_octosort PRIVATE -std=c99 -fno-strict-aliasing -Wall -O3 -march=native)
        endif()

        add_library(
            sayhisort_bench_logsort STATIC
            tests/third_party/logsort_bench.c
            tests/third_party/logsort_bench.h
            )
        target_include_directories(sayhisort_bench_logsort PRIVATE ${FETCHCONTENT_BASE_DIR}/logsort-src)
        set(SAYHISORT_BENCH_LIBS ${SAYHISORT_BENCH_LIBS} sayhisort_bench_logsort)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
            target_compile_options(sayhisort_bench_logsort PRIVATE -std=c99 -Wall -O3 -march=native)
        endif()
    endif()

    target_link_libraries(sayhisort_bench PRIVATE ${SAYHISORT_BENCH_LIBS})
    add_test(NAME sayhisort_bench COMMAND $<TARGET_FILE:sayhisort_bench>)
endif()
