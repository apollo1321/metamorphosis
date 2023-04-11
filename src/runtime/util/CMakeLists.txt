add_library(ceq_codegen codegen.cpp)
target_link_libraries(ceq_codegen ceq_lib libprotobuf libprotoc)

add_library(ceq_event event.cpp)
target_link_libraries(ceq_event ceq_lib Boost::fiber)

add_library(rpc_error rpc_error.cpp)
target_link_libraries(rpc_error ceq_lib)

add_subdirectory(cancellation)
