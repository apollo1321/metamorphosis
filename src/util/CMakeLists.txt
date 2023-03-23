add_library(ceq_util backtrace.cpp)
target_link_libraries(ceq_util Boost::stacktrace_addr2line dl)

add_subdirectory(ut)
