# ------------------------------------------------------------------------------
# rpc_generator utils
# ------------------------------------------------------------------------------

add_library(mtf_rpc_generator_util 
  util.cpp
)

target_link_libraries(mtf_rpc_generator_util
  mtf_lib
  libprotobuf
  libprotoc
)

# ------------------------------------------------------------------------------
# rpc_generator executable
# ------------------------------------------------------------------------------

add_executable(mtf_rpc_generator
  main.cpp 
  rpc_generator.cpp
)

target_link_libraries(mtf_rpc_generator
  mtf_lib
  mtf_rpc_generator_util
  mtf_rpc_generator_simulator
  mtf_rpc_generator_production
  libprotobuf
  libprotoc
)
