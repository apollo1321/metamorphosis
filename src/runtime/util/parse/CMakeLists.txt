add_library(ceq_parse parse.cpp)
target_link_libraries(ceq_parse
  ceq_lib
  ceq_chrono
  ceq_endpoint
)
