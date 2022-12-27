cmake_minimum_required(VERSION 3.15)
project(cost_effective_queue)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "Only clang compiler is currently supported")
endif()

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_ASAN "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer") 
set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread -fno-omit-frame-pointer") 

# For clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Needed by rocksdb
add_compile_options(-frtti)

include(cmake/third_party.cmake)
include(cmake/library.cmake)
include(cmake/code_generation.cmake)
include(cmake/executables.cmake)
