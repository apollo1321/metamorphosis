add_subdirectory(cancellation)

# ------------------------------------------------------------------------------
# grpc runtime
# ------------------------------------------------------------------------------

add_library(runtime_grpc
  api.cpp
  rpc_error.cpp
  rpc_server.cpp
  event.cpp
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
  Boost::fiber
  spdlog)

add_subdirectory(grpc)

# ------------------------------------------------------------------------------
# runtime simulator
# ------------------------------------------------------------------------------

add_library(runtime_simulator
  api.cpp
  rpc_error.cpp
  rpc_server.cpp
  event.cpp
  simulator/api.cpp
  simulator/rpc_client_base.cpp
  simulator/rpc_server.cpp
  simulator/rpc_service_base.cpp
  simulator/world.cpp
  simulator/host.cpp
  simulator/logger.cpp
  simulator/scheduler.cpp)

target_link_libraries(runtime_simulator
  ceq_lib
  ceq_util
  ceq_cancellation
  libprotobuf
  Boost::fiber
  GTest::gtest_main
  spdlog)

target_compile_definitions(runtime_simulator PUBLIC 
  RT_SIMULATION 
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

add_subdirectory(simulator)
