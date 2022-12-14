# ------------------------------------------------------------------------------
# runtime
# ------------------------------------------------------------------------------

add_library(runtime 
  src/runtime/rpc_client_base.cpp
  src/runtime/rpc_handler_base.cpp)

target_link_libraries(runtime 
  libprotobuf
  grpc++_reflection
  grpc++
  Boost::fiber)

target_include_directories(runtime PUBLIC src)

# ------------------------------------------------------------------------------
# store
# ------------------------------------------------------------------------------

add_library(store INTERFACE)
target_link_libraries(store INTERFACE rocksdb)
target_include_directories(store INTERFACE src ${rocksdb_SOURCE_DIR}/include)
