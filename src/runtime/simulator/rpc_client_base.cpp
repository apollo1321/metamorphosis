#include "rpc_client_base.h"

#include "world.h"

namespace ceq::rt::rpc {

ClientBase::ClientBase(const Endpoint& endpoint) noexcept : endpoint_{std::move(endpoint)} {
}

Result<SerializedData, RpcError> ClientBase::MakeRequest(const SerializedData& data,
                                                         const ServiceName& service_name,
                                                         const HandlerName& handler_name,
                                                         StopToken stop_token) noexcept {
  return sim::GetCurrentHost()->MakeRequest(endpoint_, data, service_name, handler_name,
                                            std::move(stop_token));
}

}  // namespace ceq::rt::rpc
