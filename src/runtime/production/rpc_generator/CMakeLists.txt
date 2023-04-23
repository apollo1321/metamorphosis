add_library(ceq_rpc_generator_production
  rpc_generator.cpp
)

target_link_libraries(ceq_rpc_generator_production
  ceq_rpc_generator_util
  ceq_lib
  libprotobuf
  libprotoc
)
