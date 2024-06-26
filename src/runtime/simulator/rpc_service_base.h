#pragma once

#include <runtime/rpc_server.h>
#include <runtime/util/rpc_error/rpc_error.h>
#include <util/result.h>

#include "rpc_server.h"

namespace mtf::rt::rpc {

class Server::Service {
 public:
  explicit Service(ServiceName service_name) noexcept;

  const ServiceName& GetServiceName() noexcept;

  virtual Result<SerializedData, RpcError> ProcessRequest(
      const SerializedData& data, const HandlerName& handler_name) noexcept = 0;

 protected:
  template <class Request, class Response, class Handler>
  Result<SerializedData, RpcError> ProcessRequestWrapper(const SerializedData& data,
                                                         Handler handler) noexcept {
    Request request;
    if (!request.ParseFromArray(data.data(), data.size())) {
      return Err(RpcErrorType::ParseError);
    }
    auto result = handler(request);
    if (result.HasError()) {
      return Err(std::move(result).GetError());
    }
    SerializedData serialized_result;
    serialized_result.resize(result.GetValue().ByteSizeLong());
    VERIFY(result.GetValue().SerializeToArray(serialized_result.data(), serialized_result.size()),
           "serialization error");
    return Ok(std::move(serialized_result));
  }

 private:
  const ServiceName service_name_;
};

}  // namespace mtf::rt::rpc
