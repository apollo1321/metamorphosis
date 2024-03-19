# ------------------------------------------------------------------------------
# production client
# ------------------------------------------------------------------------------

add_library(mtf_raft_client_prod client.cpp)
target_link_libraries(mtf_raft_client_prod
  mtf_lib
  mtf_raft_proto_prod
  mtf_runtime_production
  mtf_backoff
)

# ------------------------------------------------------------------------------
# simulator client
# ------------------------------------------------------------------------------

add_library(mtf_raft_client_sim client.cpp)
target_link_libraries(mtf_raft_client_sim
  mtf_lib
  mtf_raft_proto_sim
  mtf_runtime_simulator
  mtf_backoff
)
