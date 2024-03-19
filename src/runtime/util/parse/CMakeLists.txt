add_library(mtf_parse parse.cpp)
target_link_libraries(mtf_parse
  mtf_lib
  mtf_chrono
  mtf_endpoint
)
