# ------------------------------------------------------------------------------
# rpc_generator
# ------------------------------------------------------------------------------

add_executable(rpc_generator
  src/runtime/rpc_generator/main.cpp 
  src/runtime/rpc_generator/rpc_generator.cpp
  src/runtime/rpc_generator/rpc_generator.h)
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

# ------------------------------------------------------------------------------
# two-replica-queue
# ------------------------------------------------------------------------------

generate_proto(queue_service_proto2 src/two_replica_queue/queue_api.proto)

add_executable(queue_client2 src/two_replica_queue/client.cpp)
target_link_libraries(queue_client2 queue_service_proto2 CLI11::CLI11)

add_executable(queue_service2 src/two_replica_queue/service.cpp)
target_link_libraries(queue_service2 queue_service_proto2 CLI11::CLI11 store)

# ------------------------------------------------------------------------------
# replicated-queue
# ------------------------------------------------------------------------------

generate_proto(replicated_queue_proto src/queue_simulator/replicated_queue.proto)

add_executable(replicated_queue src/queue_simulator/main.cpp)
target_link_libraries(replicated_queue runtime_simulator replicated_queue_proto)
