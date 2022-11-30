cmake_minimum_required(VERSION 3.15)
project(cost_effective_queue)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_ASAN "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer") 
set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread -fno-omit-frame-pointer") 

# For clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/third_party.cmake)
include(cmake/code_generation.cmake)

generate_proto("echo_service")

add_executable(client_async src/client.cpp)
target_link_libraries(client_async echo_service Boost::fiber)
target_include_directories(client_async PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_executable(server_async src/server.cpp)
target_link_libraries(server_async echo_service Boost::fiber)
target_include_directories(server_async PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
