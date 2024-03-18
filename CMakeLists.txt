cmake_minimum_required(VERSION 3.22)
project(cost_effective_queue)

set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_ASAN "${CMAKE_CXX_FLAGS_ASAN} -g -O2 -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_TSAN} -g -O2 -fsanitize=thread -fno-omit-frame-pointer")

option(MORF_PRECOMPILE   "Compile only third party" OFF)
option(MORF_BUILD_STATIC "Build static executables" OFF)
option(MORF_FUZZ_TEST "Enable fuzz tests" OFF)

if (MORF_FUZZ_TEST)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -UNDEBUG")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-coverage=inline-8bit-counters -fsanitize-coverage=trace-cmp")
  set(FUZZTEST_FUZZING_MODE ON)
endif()

set(MORF_LOG_LEVEL OFF CACHE STRING "Set the logging level")
set_property(CACHE MORF_LOG_LEVEL PROPERTY STRINGS DEBUG INFO ERROR CRITICAL OFF)

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
  cmake_policy(SET CMP0135 NEW)
endif()

# For clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})

if (MORF_BUILD_STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++ -static")
endif()

include(third_party/third_party.cmake)
include(cmake/code_generation.cmake)

# For GTest discovery
include(GoogleTest)
enable_testing()

if (NOT MORF_PRECOMPILE)
  add_subdirectory(src)
else()
  set(precompile_targets
    Boost::fiber
    Boost::stacktrace_addr2line
    CLI11::CLI11
    GTest::gtest_main
    grpc++
    grpc++_reflection
    libprotobuf
    libprotoc
    rocksdb
    spdlog
  )

  if (MORF_FUZZ_TEST)
    list(APPEND precompile_targets fuzztest_gtest_main)
  endif()

  add_custom_target(morf_precompile DEPENDS ${precompile_targets})
endif()
