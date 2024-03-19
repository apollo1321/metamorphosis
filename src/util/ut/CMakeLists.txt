add_executable(result_test result.cpp)
target_link_libraries(result_test mtf_lib mtf_util GTest::gtest_main)
gtest_discover_tests(result_test)
