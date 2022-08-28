cmake_minimum_required(VERSION 3.15)

project(cost_effective_queue)

set(CMAKE_CXX_STANDARD 17)

# For clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
include(FetchContent)

# Google
message(STATUS "Fetching google test")
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Boost
set(BOOST_LIBRARIES config preprocessor assert mp11 winapi pool context
                static_assert throw_exception type_traits container_hash move
                core integer detail intrusive predef smart_ptr array bind
                concept_check exception function iterator mpl range regex tuple
                unordered algorithm conversion io function_types fusion utility
                optional system align atomic type_index typeof variant2
                filesystem format fiber)
set(BOOST_VERSION boost-1.80.0)

foreach(lib ${BOOST_LIBRARIES})
        message(STATUS "Fetching boost ${lib}")
        FetchContent_Declare(
                boost-${lib}
                GIT_REPOSITORY https://github.com/boostorg/${lib}.git
                GIT_TAG ${BOOST_VERSION}
        )
        FetchContent_MakeAvailable(boost-${lib})
endforeach()

# ------------------------------------------------------------------------------

add_executable(test test.cpp)
target_link_libraries(test GTest::gtest_main)
