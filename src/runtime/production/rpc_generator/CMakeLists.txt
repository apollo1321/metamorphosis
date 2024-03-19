add_library(mtf_rpc_generator_production
  rpc_generator.cpp
)

target_link_libraries(mtf_rpc_generator_production
  mtf_rpc_generator_util
  mtf_lib
  libprotobuf
  libprotoc
)
