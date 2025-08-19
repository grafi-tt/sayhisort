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
  set(CONTENT_NAME ${CONTENT_NAME} googletest)
else()
  find_package(GTest REQUIRED)
endif()

if(SAYHISORT_THIRDPARTY_BENCH)
  include(FetchContent)
  target_compile_definitions(sayhisort_bench PRIVATE SAYHISORT_THIRDPARTY_BENCH=1)

  FetchContent_Declare(
    wikisort
    GIT_REPOSITORY https://github.com/BonzaiThePenguin/WikiSort.git
    GIT_TAG df036bd85af713f5012ccc4d329f05eb6a553128
    PATCH_COMMAND git checkout . && git apply ${CMAKE_CURRENT_SOURCE_DIR}/wikisort.patch
  )
  set(CONTENT_NAME ${CONTENT_NAME} wikisort)

  add_library(
    wikisort_bench_runner STATIC
    wikisort_bench_runner.cc
    wikisort_bench_runner.h
  )
  set(BENCH_LIBS ${BENCH_LIBS} wikisort_bench_runner)
  target_include_directories(wikisort_bench_runner PRIVATE ${FETCHCONTENT_BASE_DIR}/wikisort-src)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wikisort_bench_runner PRIVATE -O3 -march=native)
  endif()

  FetchContent_Declare(
    octosort
    GIT_REPOSITORY https://github.com/scandum/octosort.git
    GIT_TAG 73605cdbdfec66e7112c6a3a3830748fcd3bb665
  )
  set(CONTENT_NAME ${CONTENT_NAME} octosort)

  add_library(
    octosort_bench_runner STATIC
    octosort_bench_runner.c
    octosort_bench_runner.h
  )
  set(BENCH_LIBS ${BENCH_LIBS} octosort_bench_runner)
  target_include_directories(octosort_bench_runner PRIVATE ${FETCHCONTENT_BASE_DIR}/octosort-src)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(octosort_bench_runner PRIVATE -std=c99 -fno-strict-aliasing -Wall -O3 -march=native)
  endif()

  FetchContent_Declare(
    logsort
    GIT_REPOSITORY https://github.com/aphitorite/Logsort.git
    GIT_TAG 96d3fcfd2e32797d4aa99d61f11a650ce47dac5a
  )
  set(CONTENT_NAME ${CONTENT_NAME} logsort)

  add_library(
    logsort_bench_runner STATIC
    logsort_bench_runner.c
    logsort_bench_runner.h
  )
  set(BENCH_LIBS ${BENCH_LIBS} logsort_bench_runner)
  target_include_directories(logsort_bench_runner PRIVATE ${FETCHCONTENT_BASE_DIR}/logsort-src)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(logsort_bench_runner PRIVATE -std=c99 -Wall -O3 -march=native)
  endif()
endif()

set(THIRDPARTY_BENCH_LIBS ${BENCH_LIBS} PARENT_SCOPE)
FetchContent_MakeAvailable(${CONTENT_NAME})
