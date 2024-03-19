add_library(mtf_cancellation
  stop_source.cpp
  stop_state.cpp
  stop_token.cpp
)

target_link_libraries(mtf_cancellation
  mtf_lib
  mtf_util
)

add_subdirectory(ut)
