add_library(ceq_runtime_base INTERFACE)
target_link_libraries(ceq_runtime_base INTERFACE
  ceq_lib
  ceq_util
  ceq_cancellation
  ceq_endpoint
  ceq_event
  ceq_rpc_error
  ceq_serde
  ceq_chrono
  ceq_print
  libprotobuf
  Boost::fiber
  spdlog
)

target_compile_definitions(ceq_runtime_base INTERFACE
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${LOG_LEVEL}
)

# ------------------------------------------------------------------------------
# production runtime
# ------------------------------------------------------------------------------

add_library(ceq_runtime_production
  database.cpp
  logger.cpp
  random.cpp
  rpc_server.cpp
  production/database.cpp
  production/logger.cpp
  production/random.cpp
  production/rpc_client_base.cpp
  production/rpc_server.cpp
  production/rpc_service_base.cpp
  production/time.cpp
)

target_link_libraries(ceq_runtime_production
  ceq_runtime_base
  grpc++_reflection
  grpc++
  rocksdb_lib
)

target_compile_definitions(ceq_runtime_production PUBLIC 
  PRODUCTION
)

add_subdirectory(production)

# ------------------------------------------------------------------------------
# simulator runtime
# ------------------------------------------------------------------------------

add_library(ceq_runtime_simulator
  database.cpp
  logger.cpp
  random.cpp
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

target_link_libraries(ceq_runtime_simulator
  ceq_runtime_base
  libprotobuf
  Boost::fiber
  spdlog
)

target_compile_definitions(ceq_runtime_simulator PUBLIC 
  SIMULATION
)

add_subdirectory(simulator)

# ------------------------------------------------------------------------------

add_subdirectory(util)
add_subdirectory(rpc_generator)
