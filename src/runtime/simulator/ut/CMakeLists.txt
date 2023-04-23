add_library(ceq_test_simulator_base INTERFACE)
target_link_libraries(ceq_test_simulator_base INTERFACE
  ceq_runtime_simulator
  GTest::gtest_main
)

generate_proto(ceq_test_simulator_rpc_proto test_service.proto)

add_library(ceq_test_simulator_base_with_proto INTERFACE)
target_link_libraries(ceq_test_simulator_base_with_proto INTERFACE
  ceq_test_simulator_base
  ceq_test_simulator_rpc_proto_sim
)

# ------------------------------------------------------------------------------

add_executable(ceq_test_simulator_time time.cpp)
target_link_libraries(ceq_test_simulator_time ceq_test_simulator_base)
gtest_discover_tests(ceq_test_simulator_time)

add_executable(ceq_test_simulator_rpc rpc.cpp)
target_link_libraries(ceq_test_simulator_rpc ceq_test_simulator_base_with_proto)
gtest_discover_tests(ceq_test_simulator_rpc)

add_executable(ceq_test_simulator_cancellation cancellation.cpp)
target_link_libraries(ceq_test_simulator_cancellation ceq_test_simulator_base)
gtest_discover_tests(ceq_test_simulator_cancellation)

add_executable(ceq_test_simulator_host_pause host_pause.cpp)
target_link_libraries(ceq_test_simulator_host_pause ceq_test_simulator_base_with_proto)
gtest_discover_tests(ceq_test_simulator_host_pause)

add_executable(ceq_test_simulator_host_kill host_kill.cpp)
target_link_libraries(ceq_test_simulator_host_kill ceq_test_simulator_base_with_proto)
gtest_discover_tests(ceq_test_simulator_host_kill)

add_executable(ceq_test_simulator_host_connections host_connections.cpp)
target_link_libraries(ceq_test_simulator_host_connections ceq_test_simulator_base_with_proto)
gtest_discover_tests(ceq_test_simulator_host_connections)

add_executable(ceq_test_simulator_database database.cpp)
target_link_libraries(ceq_test_simulator_database ceq_test_simulator_base)
gtest_discover_tests(ceq_test_simulator_database)
