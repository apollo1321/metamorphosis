#pragma once

#include <runtime/rpc_server.h>
#include <runtime/util/cancellation/stop_token.h>
#include <util/result.h>

#include "rpc_server.h"

namespace ceq::rt::rpc {

class ClientBase {
 public:
  explicit ClientBase(const Endpoint& endpoint) noexcept;

  // Non-copyable
  ClientBase(const ClientBase& other) noexcept = delete;
  ClientBase& operator=(const ClientBase& other) noexcept = delete;

  // Non-movable
  ClientBase(ClientBase&& other) noexcept = delete;
  ClientBase& operator=(ClientBase&& other) noexcept = delete;

  template <class Request, class Response>
  Result<Response, RpcError> MakeRequest(const Request& request, const ServiceName& service_name,
                                         const HandlerName& handler_name,
                                         StopToken stop_token = {}) noexcept {
    SerializedData data;
    data.resize(request.ByteSizeLong());
    VERIFY(request.SerializeToArray(data.data(), data.size()), "serialization error");

    auto result = MakeRequest(data, service_name, handler_name, stop_token);
    if (result.HasError()) {
      return Err(std::move(result).GetError());
    }

    Response proto_result;
    if (!proto_result.ParseFromArray(result.GetValue().data(), result.GetValue().size())) {
      return Err(RpcErrorType::ParseError);
    }
    return Ok(std::move(proto_result));
  }

 private:
  Result<SerializedData, RpcError> MakeRequest(const SerializedData& data,
                                               const ServiceName& service_name,
                                               const HandlerName& handler_name,
                                               StopToken stop_token = {}) noexcept;

 private:
  Endpoint endpoint_;
};

}  // namespace ceq::rt::rpc
