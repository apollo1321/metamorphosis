generate_proto(ceq_echo_service_proto echo_service.proto)

add_executable(ceq_echo_benchmark main.cpp)

target_link_libraries(ceq_echo_benchmark
  ceq_lib
  ceq_echo_service_proto_prod
  CLI11::CLI11
)
