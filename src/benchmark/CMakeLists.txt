generate_proto(echo_service_proto echo_service.proto OFF)
add_executable(echo_benchmark main.cpp)
target_link_libraries(echo_benchmark ceq_lib echo_service_proto CLI11::CLI11)
