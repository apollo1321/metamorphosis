generate_proto(ceq_test_rsm_proto rsm_msg.proto)

# ------------------------------------------------------------------------------
# test client
# ------------------------------------------------------------------------------

add_executable(ceq_raft_test_client client.cpp)
target_link_libraries(ceq_raft_test_client
  ceq_parse
  ceq_test_rsm_proto_prod
  ceq_raft_client_prod
  CLI11::CLI11
)

# ------------------------------------------------------------------------------
# test node
# ------------------------------------------------------------------------------

add_executable(ceq_raft_test_node node.cpp)
target_link_libraries(ceq_raft_test_node
  ceq_parse
  ceq_test_rsm_proto_prod
  ceq_raft_node_prod
  CLI11::CLI11
)
