generate_proto(test_rsm_proto test_rsm.proto TRUE)

add_executable(test_raft raft.cpp)
target_link_libraries(test_raft 
  raft_node 
  raft_client 
  test_rsm_proto
  runtime_simulator 
  GTest::gtest_main
)
gtest_discover_tests(test_raft)
