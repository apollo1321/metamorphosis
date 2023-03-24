add_library(ceq_lib INTERFACE)
target_include_directories(ceq_lib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory(util)
add_subdirectory(benchmark)
add_subdirectory(queue)
add_subdirectory(runtime)
add_subdirectory(codegen)
add_subdirectory(raft)
