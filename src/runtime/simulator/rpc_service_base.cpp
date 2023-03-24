#include "rpc_service_base.h"

namespace ceq::rt {

RpcServer::RpcService::RpcService(ServiceName service_name) noexcept
    : service_name_{std::move(service_name)} {
}

const ServiceName& RpcServer::RpcService::GetServiceName() noexcept {
  return service_name_;
}

}  // namespace ceq::rt
