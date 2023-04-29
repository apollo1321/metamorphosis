#include "rpc_server.h"
#include "world.h"

#include <util/defer.h>

#include "rpc_service_base.h"

namespace ceq::rt::rpc {

void Server::ServerImpl::Register(Server::Service* service) noexcept {
  VERIFY(!services_.contains(service->GetServiceName()), "service is already registered");
  services_[service->GetServiceName()] = service;
}

void Server::ServerImpl::Run(uint16_t port, const ServerRunConfig& /*config*/) noexcept {
  VERIFY(!std::exchange(running_, true), "RpcServer is already running");
  port_ = port;
  sim::GetCurrentHost()->RegisterServer(this, port);
}

void Server::ServerImpl::ShutDown() noexcept {
  VERIFY(running_, "RpcServer is not running");
  VERIFY(!std::exchange(finished_, true), "RpcServer is already finished");

  sim::GetCurrentHost()->UnregisterServer(port_);

  std::unique_lock guard(shutdown_mutex_);
  shutdown_cv_.wait(guard, [&]() {
    return running_count_ == 0;
  });
}

Result<SerializedData, RpcError> Server::ServerImpl::ProcessRequest(
    const SerializedData& data, const ServiceName& service_name,
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
    return Err(RpcErrorType::HandlerNotFound, "Unknown service: " + service_name);
  }

  return services_[service_name]->ProcessRequest(data, handler_name);
}

Server::ServerImpl::~ServerImpl() {
  VERIFY(finished_, "Destruction of running server");
}

}  // namespace ceq::rt::rpc
