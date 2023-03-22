#include "rpc_client_base.h"

#include "world.h"

namespace runtime_simulation {

RpcClientBase::RpcClientBase(Address address, Port port) noexcept
    : address_{std::move(address)}, port_{port} {
}

RpcResult RpcClientBase::MakeRequest(const SerializedData& data, const ServiceName& service_name,
                                     const HandlerName& handler_name) noexcept {
  return GetWorld()->MakeRequest(address_, port_, data, service_name, handler_name);
}

}  // namespace runtime_simulation
