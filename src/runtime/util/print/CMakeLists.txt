add_library(ceq_print print.cpp)
target_link_libraries(ceq_print
  ceq_lib
  ceq_chrono
  libprotobuf
)
