#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace ceq::rt {

using Port = uint16_t;
using Address = std::string;

struct Endpoint {
  Address address;
  Port port;
};

struct ServerRunConfig {
  ServerRunConfig() noexcept;

  size_t queue_count{};
  size_t threads_per_queue{};
  size_t worker_threads_count{};
};

class RpcServer {
 public:
  class RpcService;
  class RpcServerImpl;

 public:
  RpcServer() noexcept;

  void Register(RpcService* service) noexcept;

  void Run(Port port, ServerRunConfig run_config = ServerRunConfig()) noexcept;

  void ShutDown() noexcept;

  ~RpcServer();

 private:
  RpcServerImpl* impl_;
};

}  // namespace ceq::rt
