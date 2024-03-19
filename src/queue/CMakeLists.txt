generate_proto(mtf_queue_service_proto queue_service.proto)

# ------------------------------------------------------------------------------

add_executable(mtf_queue_client client.cpp)

target_link_libraries(mtf_queue_client
  mtf_lib
  mtf_parse
  mtf_queue_service_proto_prod
  mtf_runtime_production
  CLI11::CLI11
)

# ------------------------------------------------------------------------------

add_executable(mtf_queue_service service.cpp)

target_link_libraries(mtf_queue_service
  mtf_lib
  mtf_queue_service_proto_prod
  mtf_runtime_production
  CLI11::CLI11
)
