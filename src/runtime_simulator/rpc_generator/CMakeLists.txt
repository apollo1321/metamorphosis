add_executable(sim_rpc_generator
  main.cpp 
  rpc_generator.cpp
  rpc_generator.h)
target_link_libraries(sim_rpc_generator libprotobuf libprotoc)
