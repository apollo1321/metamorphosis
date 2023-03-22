#pragma once

#include <unordered_map>

#include <boost/fiber/all.hpp>

#include "common.h"
#include "rpc_service_base.h"

namespace runtime_simulation {

class RpcServer {
 public:
  void Register(RpcServiceBase* service) noexcept;

  void Run(uint16_t port) noexcept;

  void ShutDown() noexcept;

  ~RpcServer();

 private:
  RpcResult ProcessRequest(const SerializedData& data, const ServiceName& service_name,
                           const HandlerName& handler_name) noexcept;

  friend class Host;

 private:
  std::unordered_map<ServiceName, RpcServiceBase*> services_;

  bool running_ = false;
  bool finished_ = false;

  boost::fibers::condition_variable shutdown_cv_;
  boost::fibers::mutex shutdown_mutex_;
  size_t running_count_ = 0;

  uint16_t port_ = 0;
};

}  // namespace runtime_simulation
