#pragma once

#include <util/result.h>

#include "common.h"
#include "errors.h"

namespace runtime_simulation {

class RpcServiceBase {
 public:
  explicit RpcServiceBase(ServiceName service_name) noexcept;

  const ServiceName& GetServiceName() noexcept;

  virtual RpcResult ProcessRequest(const SerializedData& data,
                                   const HandlerName& handler_name) noexcept = 0;

 protected:
  template <class Request, class Response, class Handler>
  RpcResult ProcessRequestWrapper(const SerializedData& data, Handler handler) noexcept {
    Request request;
    if (!request.ParseFromArray(data.data(), data.size())) {
      return RpcResult::Err(RpcError::ErrorType::ParseError);
    }
    auto result = handler(request);
    if (result.HasError()) {
      return RpcResult::Err(std::move(result).ExpectError());
    }
    SerializedData serialized_result;
    serialized_result.resize(result.ExpectValue().ByteSizeLong());
    VERIFY(
        result.ExpectValue().SerializeToArray(serialized_result.data(), serialized_result.size()),
        "serialization error");
    return RpcResult::Ok(std::move(serialized_result));
  }

 private:
  const ServiceName service_name_;
};

}  // namespace runtime_simulation
