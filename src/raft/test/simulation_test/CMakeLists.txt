add_library(ceq_test_raft_base INTERFACE)
target_link_libraries(ceq_test_raft_base INTERFACE
  ceq_raft_node_sim
  ceq_raft_client_sim
  ceq_logging_state_machine_proto_sim
  ceq_logging_state_machine
  ceq_runtime_simulator 
  ceq_history_checker
  GTest::gtest_main
)

# ------------------------------------------------------------------------------

add_executable(ceq_test_raft_simple raft_simple.cpp)
target_link_libraries(ceq_test_raft_simple ceq_test_raft_base)
gtest_discover_tests(ceq_test_raft_simple)

add_executable(ceq_test_raft_host_crash raft_host_crash.cpp)
target_link_libraries(ceq_test_raft_host_crash ceq_test_raft_base)
gtest_discover_tests(ceq_test_raft_host_crash)

add_executable(ceq_test_raft_commit_index raft_commit_index.cpp)
target_link_libraries(ceq_test_raft_commit_index ceq_test_raft_base)
gtest_discover_tests(ceq_test_raft_commit_index)
