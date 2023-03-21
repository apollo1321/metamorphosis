add_executable(rpc_generator
  main.cpp 
  rpc_generator.cpp
  rpc_generator.h)
target_link_libraries(rpc_generator libprotobuf libprotoc)
