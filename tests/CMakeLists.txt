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
target_compile_features(
  sayhisort_cpp20_test PRIVATE
  cxx_std_20
)
add_test(NAME sayhisort_cpp20_test COMMAND $<TARGET_FILE:sayhisort_cpp20_test>)

add_executable(
  sayhisort_bench
  sayhisort_bench.cc
  sayhisort_bench_data.cc
  sayhisort_bench_data.h
  sayhisort_profile_util.h
  sayhisort_test_util.cc
  sayhisort_test_util.h
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

target_link_libraries(sayhisort_bench PRIVATE sayhisort ${THIRDPARTY_BENCH_LIBS})
