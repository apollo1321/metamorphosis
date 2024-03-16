#include "rpc_server.h"
#include "world.h"

#include <util/defer.h>

#include "rpc_service_base.h"

namespace mtf::rt::rpc {

void Server::ServerImpl::Register(Server::Service* service) noexcept {
  VERIFY(!services_.contains(service->GetServiceName()), "service is already registered");
  services_[service->GetServiceName()] = service;
}

void Server::ServerImpl::Start(uint16_t port) noexcept {
  VERIFY(!std::exchange(running_, true), "RpcServer is already running");
  port_ = port;
  sim::GetCurrentHost()->RegisterServer(this, port_);
}

void Server::ServerImpl::Run() noexcept {
  worker_started_.Signal();
  shutdown_event_.Await();
}

void Server::ServerImpl::ShutDown() noexcept {
  VERIFY(running_, "RpcServer is not running");
  VERIFY(!std::exchange(finished_, true), "RpcServer is already finished");

  sim::GetCurrentHost()->UnregisterServer(port_);

  requests_latch_.AwaitZero();
  shutdown_event_.Signal();
}

Result<SerializedData, RpcError> Server::ServerImpl::ProcessRequest(
    const SerializedData& data, const ServiceName& service_name,
    const HandlerName& handler_name) noexcept {
  auto guard = requests_latch_.MakeGuard();
  worker_started_.Await();

  if (!services_.contains(service_name)) {
    return Err(RpcErrorType::HandlerNotFound, "Unknown service: " + service_name);
  }

  return services_[service_name]->ProcessRequest(data, handler_name);
}

Server::ServerImpl::~ServerImpl() {
  VERIFY(finished_, "Destruction of running server");
}

}  // namespace mtf::rt::rpc
