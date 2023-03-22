#pragma once

#include <util/result.h>

#include "common.h"

namespace runtime_simulation {

class RpcClientBase {
 public:
  explicit RpcClientBase(Address address, Port port) noexcept;

  template <class Request, class Response>
  Result<Response, RpcError> MakeRequest(const Request& request, const ServiceName& service_name,
                                         const HandlerName& handler_name) noexcept {
    using ProtoResult = Result<Response, RpcError>;

    SerializedData data;
    data.resize(request.ByteSizeLong());
    VERIFY(request.SerializeToArray(data.data(), data.size()), "serialization error");

    auto result = MakeRequest(data, service_name, handler_name);
    if (result.HasError()) {
      return ProtoResult::Err(std::move(result).ExpectError());
    }

    Response proto_result;
    if (!proto_result.ParseFromArray(result.ExpectValue().data(), result.ExpectValue().size())) {
      return ProtoResult::Err(RpcError::ErrorType::ParseError);
    }
    return ProtoResult::Ok(std::move(proto_result));
  }

 private:
  RpcResult MakeRequest(const SerializedData& data, const ServiceName& service_name,
                        const HandlerName& handler_name) noexcept;

 private:
  Address address_;
  Port port_;
};

}  // namespace runtime_simulation
