cmake_minimum_required(VERSION 3.15)

project(cost_effective_queue)

set(CMAKE_CXX_STANDARD 17)

# For clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
include(FetchContent)

message(STATUS "Fetching google test")
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# ------------------------------------------------------------------------------

add_executable(test test.cpp)
target_link_libraries(test GTest::gtest_main)
