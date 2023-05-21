add_executable(ceq_history_checker_exe history_checker.cpp)
target_link_libraries(ceq_history_checker_exe
  ceq_lib
  ceq_parse
  ceq_history_checker
  CLI11::CLI11
  spdlog
)
