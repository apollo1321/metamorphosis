add_library(ceq_proto INTERFACE)
target_link_libraries(ceq_proto INTERFACE
  libprotobuf
  ceq_lib
)
