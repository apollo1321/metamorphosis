#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <boost/fiber/all.hpp>

#include <runtime/rpc_server.h>
#include <runtime/util/rpc_error/rpc_error.h>
#include <util/result.h>

namespace ceq::rt::sim {
class Host;
}

namespace ceq::rt::rpc {

using SerializedData = std::vector<uint8_t>;
using ServiceName = std::string;
using HandlerName = std::string;

class Server::ServerImpl {
 public:
  void Register(Server::Service* service) noexcept;

  void Run(uint16_t port, const ServerRunConfig& config) noexcept;

  void ShutDown() noexcept;

  ~ServerImpl();

 private:
  Result<SerializedData, Error> ProcessRequest(const SerializedData& data,
                                               const ServiceName& service_name,
                                               const HandlerName& handler_name) noexcept;

 private:
  std::unordered_map<ServiceName, Server::Service*> services_;

  bool running_ = false;
  bool finished_ = false;

  boost::fibers::condition_variable shutdown_cv_;
  boost::fibers::mutex shutdown_mutex_;
  size_t running_count_ = 0;

  uint16_t port_ = 0;

  friend class sim::Host;
};

}  // namespace ceq::rt::rpc
