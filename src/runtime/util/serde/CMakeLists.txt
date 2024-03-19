add_library(mtf_serde INTERFACE)
target_link_libraries(mtf_serde INTERFACE
  libprotobuf
  mtf_lib
)
