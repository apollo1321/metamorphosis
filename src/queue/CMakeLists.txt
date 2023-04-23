generate_proto(ceq_queue_service_proto queue_service.proto)

# ------------------------------------------------------------------------------

add_executable(ceq_queue_client client.cpp)

target_link_libraries(ceq_queue_client
  ceq_lib
  ceq_parse
  ceq_queue_service_proto_prod
  ceq_runtime_production
  CLI11::CLI11
)

# ------------------------------------------------------------------------------

add_executable(ceq_queue_service service.cpp)

target_link_libraries(ceq_queue_service
  ceq_lib
  ceq_queue_service_proto_prod
  ceq_runtime_production
  CLI11::CLI11
)
