generate_proto(queue_service_proto queue_service.proto)

add_executable(queue_client client.cpp)
target_link_libraries(queue_client ceq_lib queue_service_proto CLI11::CLI11)

add_executable(queue_service service.cpp)
target_link_libraries(queue_service ceq_lib queue_service_proto CLI11::CLI11 store)
