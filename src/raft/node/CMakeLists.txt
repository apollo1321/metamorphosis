################################################################################
# Production Node
################################################################################

add_library(mtf_raft_node_prod node.cpp state_machine_wrapper.cpp)
target_link_libraries(mtf_raft_node_prod
  mtf_lib
  mtf_raft_proto_prod
  mtf_runtime_production
)

################################################################################
# Simulator Node
################################################################################

add_library(mtf_raft_node_sim node.cpp state_machine_wrapper.cpp)
target_link_libraries(mtf_raft_node_sim
  mtf_lib
  mtf_raft_proto_sim
  mtf_runtime_simulator
)
