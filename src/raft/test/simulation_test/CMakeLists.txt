add_library(mtf_test_raft_base INTERFACE)
target_link_libraries(mtf_test_raft_base INTERFACE
  mtf_raft_node_sim
  mtf_raft_client_sim
  mtf_logging_state_machine_proto_sim
  mtf_logging_state_machine
  mtf_runtime_simulator 
  mtf_history_checker
  GTest::gtest_main
)

# ------------------------------------------------------------------------------

add_executable(mtf_test_raft_simple raft_simple.cpp)
target_link_libraries(mtf_test_raft_simple mtf_test_raft_base)
gtest_discover_tests(mtf_test_raft_simple)

add_executable(mtf_test_raft_host_crash raft_host_crash.cpp)
target_link_libraries(mtf_test_raft_host_crash mtf_test_raft_base)
gtest_discover_tests(mtf_test_raft_host_crash)

add_executable(mtf_test_raft_commit_index raft_commit_index.cpp)
target_link_libraries(mtf_test_raft_commit_index mtf_test_raft_base)
gtest_discover_tests(mtf_test_raft_commit_index)

if (MORF_FUZZ_TEST)
  add_executable(mtf_test_raft_fuzztest raft_fuzztest.cpp)
  target_link_libraries(mtf_test_raft_fuzztest PRIVATE mtf_test_raft_base)
  link_fuzztest(mtf_test_raft_fuzztest)
  gtest_discover_tests(mtf_test_raft_fuzztest)
endif()
