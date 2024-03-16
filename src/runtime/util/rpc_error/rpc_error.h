#pragma once

#include <string>

namespace mtf::rt::rpc {

enum class RpcErrorType {
  NetworkError,
  ConnectionRefused,
  HandlerNotFound,
  Internal,
  ParseError,
  Cancelled,
};

struct RpcError {
  explicit RpcError(RpcErrorType error_type = RpcErrorType::Internal,
                 const std::string& message = "") noexcept;

  std::string Message() const noexcept;

  RpcErrorType error_type;
  std::string status_message;
};

}  // namespace mtf::rt::rpc
