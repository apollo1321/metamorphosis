# ------------------------------------------------------------------------------
# runtime_simulator
# ------------------------------------------------------------------------------

add_executable(runtime_simulator_sleep_test src/runtime_simulator/ut/clock.cpp)
target_link_libraries(runtime_simulator_sleep_test runtime_simulator GTest::gtest_main)
