add_library(runtime 
  rpc_client_base.cpp
  rpc_server.cpp
  rpc_service_base.cpp)

target_link_libraries(runtime 
  ceq_lib
  ceq_util
  libprotobuf
  grpc++_reflection
  grpc++
  Boost::fiber)

add_subdirectory(rpc_generator)
