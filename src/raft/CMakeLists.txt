generate_proto(raft_proto raft.proto TRUE)

add_library(raft_node node.cpp state_machine.cpp)
target_link_libraries(raft_node ceq_lib raft_proto runtime_simulator)
# disable logging by default
target_compile_definitions(raft_node PRIVATE
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${LOG_LEVEL}
)

add_library(raft_client client.cpp)
target_link_libraries(raft_client ceq_lib raft_proto runtime_simulator)
target_compile_definitions(raft_client PRIVATE
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${LOG_LEVEL}
)

add_subdirectory(ut)
