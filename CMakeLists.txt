cmake_minimum_required(VERSION 3.24)
project(cost_effective_queue)

set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_ASAN "-g -O0 -fsanitize=address -fno-omit-frame-pointer") 
set(CMAKE_CXX_FLAGS_TSAN "-g -O2 -fsanitize=thread -fno-omit-frame-pointer") 

# For clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Needed by rocksdb
add_compile_options(-frtti)

cmake_policy(SET CMP0135 NEW)

# For GTest discovery
include(GoogleTest)

include(cmake/third_party.cmake)
include(cmake/code_generation.cmake)

enable_testing()

set(LOG_LEVEL OFF CACHE STRING "Set the logging level")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS INFO ERROR CRITICAL OFF)

add_subdirectory(src)
