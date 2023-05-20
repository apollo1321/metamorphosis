generate_proto(ceq_logging_state_machine_proto logging_state_machine.proto)

add_library(ceq_history_checker history_checker.cpp)
target_link_libraries(ceq_history_checker
  ceq_lib
  ceq_util
  ceq_chrono
)

add_library(ceq_logging_state_machine logging_state_machine.cpp)
target_link_libraries(ceq_logging_state_machine
  ceq_runtime_base
  ceq_logging_state_machine_proto
  ceq_proto
  spdlog
)

target_compile_definitions(ceq_logging_state_machine PUBLIC
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${LOG_LEVEL}
)
