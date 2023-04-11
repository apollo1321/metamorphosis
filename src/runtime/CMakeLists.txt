# ------------------------------------------------------------------------------
# production runtime
# ------------------------------------------------------------------------------

add_library(runtime_production
  database.cpp
  logger.cpp
  rpc_server.cpp
  production/database.cpp
  production/logger.cpp
  production/random.cpp
  production/rpc_client_base.cpp
  production/rpc_server.cpp
  production/rpc_service_base.cpp
  production/time.cpp
)

target_link_libraries(runtime_production
  ceq_lib
  ceq_util
  ceq_cancellation
  ceq_event
  rpc_error
  libprotobuf
  grpc++_reflection
  grpc++
  Boost::fiber
  rocksdb_lib
  spdlog
)

add_subdirectory(production)

# ------------------------------------------------------------------------------
# simulator runtime
# ------------------------------------------------------------------------------

add_library(runtime_simulator
  database.cpp
  logger.cpp
  rpc_server.cpp
  simulator/api.cpp
  simulator/database.cpp
  simulator/host.cpp
  simulator/logger.cpp
  simulator/random.cpp
  simulator/rpc_client_base.cpp
  simulator/rpc_server.cpp
  simulator/rpc_service_base.cpp
  simulator/scheduler.cpp
  simulator/time.cpp
  simulator/world.cpp
)

target_link_libraries(runtime_simulator
  ceq_lib
  ceq_util
  ceq_cancellation
  ceq_event
  rpc_error
  libprotobuf
  Boost::fiber
  GTest::gtest_main
  spdlog
)

target_compile_definitions(runtime_simulator PUBLIC 
  SIMULATION
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

add_subdirectory(simulator)

# ------------------------------------------------------------------------------

add_subdirectory(util)
