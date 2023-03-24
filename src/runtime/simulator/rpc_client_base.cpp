#include "rpc_client_base.h"

#include "world.h"

namespace ceq::rt {

RpcClientBase::RpcClientBase(const Endpoint& endpoint) noexcept : endpoint_{std::move(endpoint)} {
}

RpcResult RpcClientBase::MakeRequest(const SerializedData& data, const ServiceName& service_name,
                                     const HandlerName& handler_name) noexcept {
  return GetWorld()->MakeRequest(endpoint_, data, service_name, handler_name);
}

}  // namespace ceq::rt
