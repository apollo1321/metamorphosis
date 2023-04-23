add_library(ceq_event event.cpp)
target_link_libraries(ceq_event ceq_lib Boost::fiber)
