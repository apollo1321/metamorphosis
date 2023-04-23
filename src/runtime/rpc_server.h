#pragma once

#include <runtime/util/endpoint/endpoint.h>

#include <cstdint>
#include <string>

namespace ceq::rt::rpc {

struct ServerRunConfig {
  ServerRunConfig() noexcept;

  size_t queue_count{};
  size_t threads_per_queue{};
  size_t worker_threads_count{};
};

class Server {
 public:
  class Service;
  class ServerImpl;

 public:
  Server() noexcept;

  void Register(Service* service) noexcept;

  void Run(Port port, ServerRunConfig run_config = ServerRunConfig()) noexcept;

  void ShutDown() noexcept;

  ~Server();

 private:
  ServerImpl* impl_;
};

}  // namespace ceq::rt::rpc
