#pragma once

#include <runtime/rpc_server.h>
#include <runtime/util/event/event.h>
#include <runtime/util/latch/latch.h>
#include <runtime/util/rpc_error/rpc_error.h>
#include <util/result.h>

#include <boost/fiber/all.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace mtf::rt::sim {
class Host;
}

namespace mtf::rt::rpc {

using SerializedData = std::vector<uint8_t>;
using ServiceName = std::string;
using HandlerName = std::string;

class Server::ServerImpl {
 public:
  void Register(Server::Service* service) noexcept;

  void Start(Port port) noexcept;

  void Run() noexcept;

  void ShutDown() noexcept;

  ~ServerImpl();

 private:
  Result<SerializedData, RpcError> ProcessRequest(const SerializedData& data,
                                                  const ServiceName& service_name,
                                                  const HandlerName& handler_name) noexcept;

 private:
  std::unordered_map<ServiceName, Server::Service*> services_;

  bool running_ = false;
  bool finished_ = false;

  Latch requests_latch_;
  Latch workers_latch_;

  Event worker_started_;
  Event shutdown_event_;

  uint16_t port_ = 0;

  friend class sim::Host;
};

}  // namespace mtf::rt::rpc
