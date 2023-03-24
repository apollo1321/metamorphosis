#pragma once

#include <runtime/rpc_server.h>
#include <boost/fiber/all.hpp>
#include <unordered_map>

#include "common.h"

namespace ceq::rt {

class RpcServer::RpcServerImpl {
 public:
  void Register(RpcServer::RpcService* service) noexcept;

  void Run(uint16_t port, const ServerRunConfig& config) noexcept;

  void ShutDown() noexcept;

  ~RpcServerImpl();

 private:
  RpcResult ProcessRequest(const SerializedData& data, const ServiceName& service_name,
                           const HandlerName& handler_name) noexcept;

 private:
  std::unordered_map<ServiceName, RpcServer::RpcService*> services_;

  bool running_ = false;
  bool finished_ = false;

  boost::fibers::condition_variable shutdown_cv_;
  boost::fibers::mutex shutdown_mutex_;
  size_t running_count_ = 0;

  uint16_t port_ = 0;

  friend class Host;
};

}  // namespace ceq::rt
