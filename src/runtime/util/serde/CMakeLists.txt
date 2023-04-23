add_library(ceq_serde INTERFACE)
target_link_libraries(ceq_serde INTERFACE
  libprotobuf
  ceq_lib
)
