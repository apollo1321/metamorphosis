add_library(mtf_proto conversion.cpp)
target_link_libraries(mtf_proto PUBLIC
  libprotobuf
  mtf_lib
)
