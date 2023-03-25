add_subdirectory(cancellation)

# ------------------------------------------------------------------------------
# grpc runtime
# ------------------------------------------------------------------------------

add_library(runtime_grpc
  rpc_error.cpp
  rpc_server.cpp
  grpc/api.cpp
  grpc/rpc_client_base.cpp
  grpc/rpc_server.cpp
  grpc/rpc_service_base.cpp)

target_link_libraries(runtime_grpc
  ceq_lib
  ceq_util
  ceq_cancellation
  libprotobuf
  grpc++_reflection
  grpc++
  Boost::fiber)

add_subdirectory(grpc)

# ------------------------------------------------------------------------------
# runtime simulator
# ------------------------------------------------------------------------------

add_library(runtime_simulator
  rpc_error.cpp
  rpc_server.cpp
  simulator/api.cpp
  simulator/rpc_client_base.cpp
  simulator/rpc_server.cpp
  simulator/rpc_service_base.cpp
  simulator/world.cpp
  simulator/host.cpp
  simulator/event.cpp
  simulator/scheduler.cpp)

target_link_libraries(runtime_simulator
  ceq_lib
  ceq_util
  ceq_cancellation
  libprotobuf
  Boost::fiber)

target_compile_definitions(runtime_simulator PUBLIC RT_SIMULATION)

add_subdirectory(simulator)
