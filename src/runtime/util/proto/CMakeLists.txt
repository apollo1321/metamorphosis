add_library(mtf_proto INTERFACE)
target_link_libraries(mtf_proto INTERFACE
  libprotobuf
  mtf_lib
)
