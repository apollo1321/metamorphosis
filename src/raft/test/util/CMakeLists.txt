generate_proto(mtf_logging_state_machine_proto logging_state_machine.proto)

add_library(mtf_history_checker history_checker.cpp)
target_link_libraries(mtf_history_checker
  mtf_lib
  mtf_util
  mtf_chrono
)

add_library(mtf_logging_state_machine logging_state_machine.cpp)
target_link_libraries(mtf_logging_state_machine
  mtf_runtime_base
  mtf_logging_state_machine_proto
  mtf_proto
  spdlog
)

target_compile_definitions(mtf_logging_state_machine PUBLIC
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${MORF_LOG_LEVEL}
)
