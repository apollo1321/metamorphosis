#pragma once

#include <runtime/cancellation/stop_token.h>
#include <runtime/rpc_server.h>
#include <util/result.h>

#include "common.h"

namespace ceq::rt {

class RpcClientBase {
 public:
  explicit RpcClientBase(const Endpoint& endpoint) noexcept;

  template <class Request, class Response>
  Result<Response, RpcError> MakeRequest(const Request& request, ServiceName service_name,
                                         HandlerName handler_name,
                                         StopToken stop_token = {}) noexcept {
    SerializedData data;
    data.resize(request.ByteSizeLong());
    VERIFY(request.SerializeToArray(data.data(), data.size()), "serialization error");

    auto result = MakeRequest(std::move(data), std::move(service_name), std::move(handler_name),
                              std::move(stop_token));
    if (result.HasError()) {
      return Err(std::move(result).GetError());
    }

    Response proto_result;
    if (!proto_result.ParseFromArray(result.GetValue().data(), result.GetValue().size())) {
      return Err(RpcError::ErrorType::ParseError);
    }
    return Ok(std::move(proto_result));
  }

 private:
  RpcResult MakeRequest(SerializedData data, ServiceName service_name, HandlerName handler_name,
                        StopToken stop_token = {}) noexcept;

 private:
  Endpoint endpoint_;
};

}  // namespace ceq::rt
