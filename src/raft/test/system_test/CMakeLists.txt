add_executable(mtf_history_checker_exe history_checker.cpp)
target_link_libraries(mtf_history_checker_exe
  mtf_lib
  mtf_parse
  mtf_history_checker
  CLI11::CLI11
  spdlog
)
