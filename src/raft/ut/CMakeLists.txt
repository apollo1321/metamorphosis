generate_proto(test_rsm_proto test_rsm.proto TRUE)

add_executable(test_raft_simple raft_simple.cpp)
target_link_libraries(test_raft_simple 
  raft_node 
  raft_client 
  test_rsm_proto
  runtime_simulator 
  GTest::gtest_main
)
target_compile_definitions(test_raft_simple PRIVATE
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${LOG_LEVEL}
)
gtest_discover_tests(test_raft_simple)

add_executable(test_raft_host_crash raft_host_crash.cpp)
target_link_libraries(test_raft_host_crash 
  raft_node 
  raft_client 
  test_rsm_proto
  runtime_simulator 
  GTest::gtest_main
)
target_compile_definitions(test_raft_host_crash PRIVATE
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${LOG_LEVEL}
)
gtest_discover_tests(test_raft_host_crash)
