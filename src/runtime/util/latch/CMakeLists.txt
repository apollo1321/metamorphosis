add_library(ceq_latch latch.cpp)
target_link_libraries(ceq_latch ceq_lib Boost::fiber)
