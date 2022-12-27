# ------------------------------------------------------------------------------
# rpc_generator
# ------------------------------------------------------------------------------

add_executable(rpc_generator
  src/rpc_generator/main.cpp 
  src/rpc_generator/rpc_generator.cpp
  src/rpc_generator/rpc_generator.h)
target_link_libraries(rpc_generator libprotobuf libprotoc)

# ------------------------------------------------------------------------------
# benchmark
# ------------------------------------------------------------------------------

generate_proto(echo_service_proto src/benchmark/echo_service.proto)
add_executable(echo_benchmark src/benchmark/main.cpp)
target_link_libraries(echo_benchmark echo_service_proto CLI11::CLI11)

# ------------------------------------------------------------------------------
# queue
# ------------------------------------------------------------------------------

generate_proto(queue_service_proto src/queue/queue_service.proto)

add_executable(queue_client src/queue/client.cpp)
target_link_libraries(queue_client queue_service_proto CLI11::CLI11)

add_executable(queue_service src/queue/service.cpp)
target_link_libraries(queue_service queue_service_proto CLI11::CLI11 store)
