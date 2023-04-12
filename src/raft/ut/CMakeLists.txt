add_executable(test_raft raft.cpp)
target_link_libraries(test_raft 
  raft_node 
  raft_client 
  runtime_simulator 
  GTest::gtest_main
)
gtest_discover_tests(test_raft)
