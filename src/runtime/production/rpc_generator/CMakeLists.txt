add_executable(production_rpc_generator
  main.cpp 
  rpc_generator.cpp
  rpc_generator.h
)

target_link_libraries(production_rpc_generator
  ceq_lib
  ceq_codegen
  libprotobuf
  libprotoc
)
