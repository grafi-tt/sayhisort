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
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()
