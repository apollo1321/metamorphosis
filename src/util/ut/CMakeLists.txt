add_executable(result_test result.cpp)
target_link_libraries(result_test ceq_lib ceq_util GTest::gtest_main)
gtest_discover_tests(result_test)
