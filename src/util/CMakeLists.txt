add_library(mtf_util backtrace.cpp)
target_link_libraries(mtf_util PRIVATE
  Boost::stacktrace_addr2line
  dl
)

add_subdirectory(ut)
