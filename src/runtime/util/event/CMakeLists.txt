add_library(mtf_event event.cpp)
target_link_libraries(mtf_event mtf_lib Boost::fiber)
