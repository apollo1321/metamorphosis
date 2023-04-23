add_library(ceq_rpc_generator_simulator
  rpc_generator.cpp
)

target_link_libraries(ceq_rpc_generator_simulator
  ceq_rpc_generator_util
  ceq_lib
  libprotobuf
  libprotoc
)
