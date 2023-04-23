# ------------------------------------------------------------------------------
# production node
# ------------------------------------------------------------------------------

add_library(ceq_raft_node_prod node.cpp state_machine.cpp)
target_link_libraries(ceq_raft_node_prod
  ceq_lib
  ceq_raft_proto_prod
  ceq_runtime_production
)

# ------------------------------------------------------------------------------
# simulator node
# ------------------------------------------------------------------------------

add_library(ceq_raft_node_sim node.cpp state_machine.cpp)
target_link_libraries(ceq_raft_node_sim
  ceq_lib
  ceq_raft_proto_sim
  ceq_runtime_simulator
)
