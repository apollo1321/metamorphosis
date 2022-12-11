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

add_executable(echo_benchmark src/benchmark/main.cpp)
target_link_libraries(echo_benchmark echo_service)

generate_proto("queue")

add_executable(queue_client src/queue/client.cpp)
target_link_libraries(queue_client queue CLI11::CLI11)

add_executable(queue_service src/queue/service.cpp)
target_link_libraries(queue_service queue CLI11::CLI11)
