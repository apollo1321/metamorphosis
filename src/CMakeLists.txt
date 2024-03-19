add_library(mtf_lib INTERFACE)
target_include_directories(mtf_lib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory(util)
add_subdirectory(benchmark)
add_subdirectory(queue)
add_subdirectory(runtime)
add_subdirectory(raft)
add_subdirectory(metamorphosis)
