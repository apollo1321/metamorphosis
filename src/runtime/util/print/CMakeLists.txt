add_library(mtf_print print.cpp)
target_link_libraries(mtf_print
  mtf_lib
  mtf_chrono
  libprotobuf
)
