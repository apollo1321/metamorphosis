#include "rpc_client_base.h"

#include "world.h"

namespace ceq::rt {

RpcClientBase::RpcClientBase(const Endpoint& endpoint) noexcept : endpoint_{std::move(endpoint)} {
}

RpcResult RpcClientBase::MakeRequest(SerializedData data, ServiceName service_name,
                                     HandlerName handler_name, StopToken stop_token) noexcept {
  return GetWorld()->MakeRequest(endpoint_, std::move(data), std::move(service_name),
                                 std::move(handler_name), std::move(stop_token));
}

}  // namespace ceq::rt
