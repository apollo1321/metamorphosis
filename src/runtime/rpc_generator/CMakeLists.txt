# ------------------------------------------------------------------------------
# rpc_generator utils
# ------------------------------------------------------------------------------

add_library(ceq_rpc_generator_util 
  util.cpp
)

target_link_libraries(ceq_rpc_generator_util
  ceq_lib
  libprotobuf
  libprotoc
)

# ------------------------------------------------------------------------------
# rpc_generator executable
# ------------------------------------------------------------------------------

add_executable(ceq_rpc_generator
  main.cpp 
  rpc_generator.cpp
)

target_link_libraries(ceq_rpc_generator
  ceq_lib
  ceq_rpc_generator_util
  ceq_rpc_generator_simulator
  ceq_rpc_generator_production
  libprotobuf
  libprotoc
)
