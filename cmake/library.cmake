# ------------------------------------------------------------------------------
# runtime
# ------------------------------------------------------------------------------

add_library(runtime 
  src/runtime/rpc_client_base.cpp
  src/runtime/rpc_server.cpp
  src/runtime/rpc_service_base.cpp)

target_link_libraries(runtime 
  libprotobuf
  grpc++_reflection
  grpc++
  Boost::fiber)

target_include_directories(runtime PUBLIC src)

# ------------------------------------------------------------------------------
# runtime_simulator
# ------------------------------------------------------------------------------

add_library(runtime_simulator
  src/runtime_simulator/api.cpp
  src/runtime_simulator/rpc_client_base.cpp
  src/runtime_simulator/rpc_server.cpp
  src/runtime_simulator/rpc_service_base.cpp
  src/runtime_simulator/impl/world.cpp
  src/runtime_simulator/impl/host.cpp)

target_link_libraries(runtime_simulator
  libprotobuf
  Boost::fiber)

target_include_directories(runtime_simulator PUBLIC src)

# ------------------------------------------------------------------------------
# store
# ------------------------------------------------------------------------------

add_library(store INTERFACE)
target_link_libraries(store INTERFACE rocksdb)
target_include_directories(store INTERFACE src ${rocksdb_SOURCE_DIR}/include)
