#include "rpc_server.h"
#include "world.h"

#include <util/defer.h>

namespace runtime_simulation {

void RpcServer::Register(RpcServiceBase* service) noexcept {
  VERIFY(!services_.contains(service->GetServiceName()), "service is already registered");
  services_[service->GetServiceName()] = service;
}

void RpcServer::Run(uint16_t port) noexcept {
  VERIFY(!std::exchange(running_, true), "RpcServer is already running");
  port_ = port;
  GetCurrentHost()->RegisterServer(this, port);
}

void RpcServer::ShutDown() noexcept {
  VERIFY(running_, "RpcServer is not running");
  VERIFY(!std::exchange(finished_, true), "RpcServer is already finished");

  GetCurrentHost()->UnregisterServer(port_);

  std::unique_lock guard(shutdown_mutex_);
  shutdown_cv_.wait(guard, [&]() {
    return running_count_ == 0;
  });
}

RpcResult RpcServer::ProcessRequest(const SerializedData& data, const ServiceName& service_name,
                                    const HandlerName& handler_name) noexcept {
  {
    std::lock_guard guard(shutdown_mutex_);
    ++running_count_;
  }

  DEFER {
    std::lock_guard guard(shutdown_mutex_);
    --running_count_;
  };

  if (!services_.contains(service_name)) {
    return RpcResult::Err(RpcError::ErrorType::ServiceNotFound, "Unknown service: " + service_name);
  }

  return services_[service_name]->ProcessRequest(data, handler_name);
}

RpcServer::~RpcServer() {
  if (!finished_) {
    ShutDown();
  }
}

}  // namespace runtime_simulation
