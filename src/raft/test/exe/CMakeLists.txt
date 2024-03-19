# ------------------------------------------------------------------------------
# test client
# ------------------------------------------------------------------------------

add_executable(mtf_raft_test_client client.cpp)
target_link_libraries(mtf_raft_test_client
  mtf_parse
  mtf_logging_state_machine_proto_prod
  mtf_raft_client_prod
  CLI11::CLI11
)

# ------------------------------------------------------------------------------
# test node
# ------------------------------------------------------------------------------

add_executable(mtf_raft_test_node node.cpp)
target_link_libraries(mtf_raft_test_node
  mtf_parse
  mtf_raft_node_prod
  mtf_logging_state_machine_proto_prod
  mtf_logging_state_machine
  CLI11::CLI11
)
