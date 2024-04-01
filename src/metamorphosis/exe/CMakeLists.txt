################################################################################
# Test Client
################################################################################

add_executable(mtf_client_exe client.cpp)
target_link_libraries(mtf_client_exe
  mtf_parse
  mtf_proto
  mtf_metamorphosis_proto_prod
  CLI11::CLI11
)

################################################################################
# Test Node
################################################################################

add_executable(mtf_node_exe node.cpp)
target_link_libraries(mtf_node_exe
  mtf_parse
  mtf_node_prod
  CLI11::CLI11
)
