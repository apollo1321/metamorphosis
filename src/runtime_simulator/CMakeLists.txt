add_library(runtime_simulator
  api.cpp
  rpc_client_base.cpp
  rpc_server.cpp
  rpc_service_base.cpp
  impl/world.cpp
  impl/host.cpp
  impl/event.cpp
  impl/scheduler.cpp)

target_link_libraries(runtime_simulator
  ceq_lib
  libprotobuf
  Boost::fiber)

target_include_directories(runtime_simulator PUBLIC src)

add_subdirectory(ut)
