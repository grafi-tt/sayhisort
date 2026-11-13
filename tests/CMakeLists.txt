cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

enable_testing()

add_executable(
  sayhisort_test
  sayhisort_test.cc
  sayhisort_test_util.cc
  sayhisort_test_util.h
)
add_test(NAME sayhisort_test COMMAND $<TARGET_FILE:sayhisort_test>)

add_executable(
  sayhisort_cpp20_test
  sayhisort_cpp20_test.cc
  sayhisort_test_util.cc
  sayhisort_test_util.h
)
target_compile_features(sayhisort_cpp20_test PRIVATE cxx_std_20)
add_test(NAME sayhisort_cpp20_test COMMAND $<TARGET_FILE:sayhisort_cpp20_test>)

add_executable(
  sayhisort_random_test
  sayhisort_random_test.cc
  sayhisort_fuzz_util.h
)
add_test(NAME sayhisort_random_test COMMAND $<TARGET_FILE:sayhisort_random_test>)

add_executable(
  sayhisort_bench
  sayhisort_bench.cc
  sayhisort_bench_data.cc
  sayhisort_bench_data.h
  sayhisort_profile_util.h
  sayhisort_test_util.cc
  sayhisort_test_util.h
)
target_compile_features(sayhisort_bench PRIVATE cxx_std_20)
add_test(NAME sayhisort_bench COMMAND $<TARGET_FILE:sayhisort_bench>)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(sayhisort_test PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(sayhisort_cpp20_test PRIVATE -std=c++20 -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(sayhisort_random_test PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(sayhisort_bench PRIVATE -std=c++20 -Wall -Wextra -Wpedantic -Werror -O3 -march=native)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(sayhisort_bench PRIVATE /Zc:preprocessor)
endif()

add_library(
  sayhisort_bench_runner OBJECT
  sayhisort_bench_runner.cc
  sayhisort_bench_runner.h
)
target_link_libraries(sayhisort_bench_runner sayhisort)
target_compile_features(sayhisort_bench_runner PRIVATE cxx_std_20)

add_library(
  sayhisort_profile_bench_runner OBJECT
  sayhisort_profile_bench_runner.cc
  sayhisort_profile_bench_runner.h
  sayhisort_profile_util.h
)
target_link_libraries(sayhisort_profile_bench_runner sayhisort)
target_compile_features(sayhisort_profile_bench_runner PRIVATE cxx_std_20)

add_library(
  stablesort_bench_runner OBJECT
  stablesort_bench_runner.cc
  stablesort_bench_runner.h
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(sayhisort_bench_runner PRIVATE -std=c++20 -Wall -Wextra -Wpedantic -Werror -O3 -march=native)
  target_compile_options(sayhisort_profile_bench_runner PRIVATE -std=c++20 -Wall -Wextra -Wpedantic -Werror -O3 -march=native)
  target_compile_options(stablesort_bench_runner PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror -O3 -march=native)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(sayhisort_bench_runner PRIVATE /Zc:preprocessor)
  target_compile_options(sayhisort_profile_bench_runner PRIVATE /Zc:preprocessor)
endif()

add_subdirectory(third_party)

target_link_libraries(
  sayhisort_test PRIVATE
  sayhisort
  GTest::gtest_main
)
target_link_libraries(
  sayhisort_cpp20_test PRIVATE
  sayhisort
  GTest::gtest_main
)
target_link_libraries(sayhisort_random_test PRIVATE sayhisort)

if(SAYHISORT_ENABLE_FUZZER)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "SAYHISORT_ENABLE_FUZZER requires Clang")
  endif()
  add_executable(
    sayhisort_fuzzer
    sayhisort_fuzzer.cc
    sayhisort_fuzz_util.h
  )
  target_compile_options(sayhisort_fuzzer PRIVATE -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsanitize=fuzzer)
  target_link_libraries(sayhisort_fuzzer PRIVATE sayhisort -fsanitize=fuzzer)
endif()

# Apply sanitizers to test targets
if(SAYHISORT_SANITIZE)
  if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "SAYHISORT_SANITIZE requires GCC or Clang")
  endif()
  set(SANITIZE_FLAGS -fsanitize=${SAYHISORT_SANITIZE} -fno-sanitize-recover=all -fno-omit-frame-pointer)
  set(SANITIZE_TARGETS sayhisort_test sayhisort_cpp20_test sayhisort_random_test)
  if(SAYHISORT_ENABLE_FUZZER)
    list(APPEND SANITIZE_TARGETS sayhisort_fuzzer)
  endif()
  foreach(target IN LISTS SANITIZE_TARGETS)
    target_compile_options(${target} PRIVATE ${SANITIZE_FLAGS})
    target_link_libraries(${target} PRIVATE ${SANITIZE_FLAGS})
  endforeach()
endif()

target_link_libraries(sayhisort_bench PRIVATE
  sayhisort
  sayhisort_bench_runner
  sayhisort_profile_bench_runner
  stablesort_bench_runner
  ${THIRDPARTY_BENCH_LIBS}
)
