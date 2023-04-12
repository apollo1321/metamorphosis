generate_proto(raft_proto raft.proto TRUE)

add_library(raft_node node.cpp state_machine.cpp)
target_link_libraries(raft_node ceq_lib raft_proto runtime_simulator)

add_library(raft_client client.cpp)
target_link_libraries(raft_client ceq_lib raft_proto runtime_simulator)

add_subdirectory(ut)
