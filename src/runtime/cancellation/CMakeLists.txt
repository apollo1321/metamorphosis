add_library(ceq_cancellation
  stop_callback.cpp
  stop_source.cpp
  stop_state.cpp
  stop_token.cpp)

target_link_libraries(ceq_cancellation
  ceq_lib
  ceq_util)

add_subdirectory(ut)
