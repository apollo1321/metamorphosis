add_library(mtf_test_production_base INTERFACE)
target_link_libraries(mtf_test_production_base INTERFACE
  mtf_runtime_production
  GTest::gtest_main
)

generate_proto(mtf_test_production_rpc_proto test_service.proto)

add_library(mtf_test_production_base_with_proto INTERFACE)
target_link_libraries(mtf_test_production_base_with_proto INTERFACE
  mtf_test_production_base
  mtf_test_production_rpc_proto_prod
)

# ------------------------------------------------------------------------------

add_executable(mtf_test_production_cancellation cancellation.cpp)
target_link_libraries(mtf_test_production_cancellation mtf_test_production_base)
gtest_discover_tests(mtf_test_production_cancellation)


add_executable(mtf_test_production_rpc rpc.cpp)
target_link_libraries(mtf_test_production_rpc mtf_test_production_base_with_proto)
gtest_discover_tests(mtf_test_production_rpc)

add_executable(mtf_test_production_database database.cpp)
target_link_libraries(mtf_test_production_database mtf_test_production_base)
gtest_discover_tests(mtf_test_production_database)
