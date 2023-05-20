cmake_minimum_required(VERSION 3.22)
project(cost_effective_queue)

set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_ASAN "-g -O1 -fsanitize=address -fno-omit-frame-pointer") 
set(CMAKE_CXX_FLAGS_TSAN "-g -O1 -fsanitize=thread -fno-omit-frame-pointer") 

# For clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# For GTest discovery
include(GoogleTest)

include(cmake/third_party.cmake)
include(cmake/code_generation.cmake)

enable_testing()

set(LOG_LEVEL ERROR CACHE STRING "Set the logging level")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS INFO ERROR CRITICAL OFF)

option(MORF_PRECOMPILE "Compile only third party" OFF)
option(MORF_BUILD_STATIC "Build static executables" OFF)

if (MORF_BUILD_STATIC) 
  set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

if (NOT MORF_PRECOMPILE)
  add_subdirectory(src)
endif()
