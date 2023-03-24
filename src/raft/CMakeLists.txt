generate_proto(raft_proto raft.proto TRUE)

add_library(raft_node node.cpp)
target_link_libraries(raft_node ceq_lib raft_proto runtime_simulator)
