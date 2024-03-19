add_library(mtf_test_simulator_base INTERFACE)
target_link_libraries(mtf_test_simulator_base INTERFACE
  mtf_runtime_simulator
  GTest::gtest_main
)

generate_proto(mtf_test_simulator_rpc_proto test_service.proto)

add_library(mtf_test_simulator_base_with_proto INTERFACE)
target_link_libraries(mtf_test_simulator_base_with_proto INTERFACE
  mtf_test_simulator_base
  mtf_test_simulator_rpc_proto_sim
)

# ------------------------------------------------------------------------------

add_executable(mtf_test_simulator_time time.cpp)
target_link_libraries(mtf_test_simulator_time mtf_test_simulator_base)
gtest_discover_tests(mtf_test_simulator_time)

add_executable(mtf_test_simulator_rpc rpc.cpp)
target_link_libraries(mtf_test_simulator_rpc mtf_test_simulator_base_with_proto)
gtest_discover_tests(mtf_test_simulator_rpc)

add_executable(mtf_test_simulator_cancellation cancellation.cpp)
target_link_libraries(mtf_test_simulator_cancellation mtf_test_simulator_base)
gtest_discover_tests(mtf_test_simulator_cancellation)

add_executable(mtf_test_simulator_host_pause host_pause.cpp)
target_link_libraries(mtf_test_simulator_host_pause mtf_test_simulator_base_with_proto)
gtest_discover_tests(mtf_test_simulator_host_pause)

add_executable(mtf_test_simulator_host_kill host_kill.cpp)
target_link_libraries(mtf_test_simulator_host_kill mtf_test_simulator_base_with_proto)
gtest_discover_tests(mtf_test_simulator_host_kill)

add_executable(mtf_test_simulator_host_connections host_connections.cpp)
target_link_libraries(mtf_test_simulator_host_connections mtf_test_simulator_base_with_proto)
gtest_discover_tests(mtf_test_simulator_host_connections)

add_executable(mtf_test_simulator_database database.cpp)
target_link_libraries(mtf_test_simulator_database mtf_test_simulator_base)
gtest_discover_tests(mtf_test_simulator_database)
