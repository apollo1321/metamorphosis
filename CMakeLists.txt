cmake_minimum_required(VERSION 3.15)

project(cost_effective_queue)

set(CMAKE_CXX_STANDARD 17)

# For clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
cmake_policy(SET CMP0135 NEW)

# Google
message(STATUS "Fetching googletest")
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
)
FetchContent_MakeAvailable(googletest)

message(STATUS "Fetching grpc")
FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG v1.48.1
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(gRPC)

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
                GIT_SHALLOW ON
        )
        FetchContent_MakeAvailable(boost-${lib})
endforeach()

# ------------------------------------------------------------------------------
# Proto compilation
# ------------------------------------------------------------------------------

get_filename_component(proto "protos/messages.proto" ABSOLUTE)
get_filename_component(proto_path ${proto} PATH)

set(proto_dir "${CMAKE_CURRENT_BINARY_DIR}/proto")
set(proto_srcs "${proto_dir}/messages.pb.cc")
set(proto_hdrs "${proto_dir}/messages.pb.h")
set(grpc_srcs "${proto_dir}/messages.grpc.pb.cc")
set(grpc_hdrs "${proto_dir}/messages.grpc.pb.h")

add_custom_command(
  OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
  COMMAND $<TARGET_FILE:protoc>
  ARGS --grpc_out "${proto_dir}"
    --cpp_out "${proto_dir}"
    -I "${proto_path}"
    --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
    "${proto}"
  DEPENDS "${proto}"
)

add_library(grpc_proto
  ${grpc_srcs}
  ${grpc_hdrs}
  ${proto_srcs}
  ${proto_hdrs}
)
target_link_libraries(grpc_proto
  grpc++_reflection
  grpc++
  libprotobuf
)

# ------------------------------------------------------------------------------
# Client-server compilation
# ------------------------------------------------------------------------------

add_executable(client src/client.cpp)
target_link_libraries(client grpc_proto)
target_include_directories(client PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_executable(server src/server.cpp)
target_link_libraries(server grpc_proto)
target_include_directories(server PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

