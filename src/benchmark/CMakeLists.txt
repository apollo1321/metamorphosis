generate_proto(mtf_echo_service_proto echo_service.proto)

add_executable(mtf_echo_benchmark main.cpp)

target_link_libraries(mtf_echo_benchmark
  mtf_lib
  mtf_echo_service_proto_prod
  CLI11::CLI11
)
