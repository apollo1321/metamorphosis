#include "rpc_service_base.h"

namespace runtime_simulation {

RpcServiceBase::RpcServiceBase(ServiceName service_name) noexcept
    : service_name_{std::move(service_name)} {
}

const ServiceName& RpcServiceBase::GetServiceName() noexcept {
  return service_name_;
}

}  // namespace runtime_simulation
