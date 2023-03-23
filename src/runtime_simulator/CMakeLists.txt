add_library(runtime_simulator
  api.cpp
  rpc_client_base.cpp
  rpc_server.cpp
  rpc_service_base.cpp
  world.cpp
  host.cpp
  event.cpp
  scheduler.cpp
  errors.cpp)

target_link_libraries(runtime_simulator
  ceq_lib
  ceq_util
  libprotobuf
  Boost::fiber)

add_subdirectory(rpc_generator)
add_subdirectory(ut)
