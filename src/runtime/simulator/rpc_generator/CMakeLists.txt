add_library(mtf_rpc_generator_simulator
  rpc_generator.cpp
)

target_link_libraries(mtf_rpc_generator_simulator
  mtf_rpc_generator_util
  mtf_lib
  libprotobuf
  libprotoc
)
