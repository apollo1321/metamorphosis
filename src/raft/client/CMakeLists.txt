# ------------------------------------------------------------------------------
# production client
# ------------------------------------------------------------------------------

add_library(ceq_raft_client_prod client.cpp)
target_link_libraries(ceq_raft_client_prod
  ceq_lib
  ceq_raft_proto_prod
  ceq_runtime_production
  ceq_backoff
)

# ------------------------------------------------------------------------------
# simulator client
# ------------------------------------------------------------------------------

add_library(ceq_raft_client_sim client.cpp)
target_link_libraries(ceq_raft_client_sim
  ceq_lib
  ceq_raft_proto_sim
  ceq_runtime_simulator
  ceq_backoff
)
