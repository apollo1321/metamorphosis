################################################################################
# Production Node
################################################################################

add_library(mtf_node_prod node.cpp)
target_link_libraries(mtf_node_prod
  mtf_lib
  mtf_hash
  mtf_serde
  mtf_metamorphosis_proto_prod
  mtf_runtime_production
)

################################################################################
# Simulator Node
################################################################################

add_library(mtf_node_sim node.cpp)
target_link_libraries(mtf_node_sim
  mtf_lib
  mtf_hash
  mtf_serde
  mtf_metamorphosis_proto_sim
  mtf_runtime_simulator
)
