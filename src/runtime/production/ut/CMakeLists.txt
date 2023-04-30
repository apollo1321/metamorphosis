add_library(ceq_test_production_base INTERFACE)
target_link_libraries(ceq_test_production_base INTERFACE
  ceq_runtime_production
  GTest::gtest_main
)

generate_proto(ceq_test_production_rpc_proto test_service.proto)

add_library(ceq_test_production_base_with_proto INTERFACE)
target_link_libraries(ceq_test_production_base_with_proto INTERFACE
  ceq_test_production_base
  ceq_test_production_rpc_proto_prod
)

# ------------------------------------------------------------------------------

add_executable(ceq_test_production_cancellation cancellation.cpp)
target_link_libraries(ceq_test_production_cancellation ceq_test_production_base)
gtest_discover_tests(ceq_test_production_cancellation)


add_executable(ceq_test_production_rpc rpc.cpp)
target_link_libraries(ceq_test_production_rpc ceq_test_production_base_with_proto)
gtest_discover_tests(ceq_test_production_rpc)

add_executable(ceq_test_production_database database.cpp)
target_link_libraries(ceq_test_production_database ceq_test_production_base)
gtest_discover_tests(ceq_test_production_database)
