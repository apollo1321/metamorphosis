add_library(mtf_latch latch.cpp)
target_link_libraries(mtf_latch mtf_lib Boost::fiber)
