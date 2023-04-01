#pragma once

#include <string>

namespace ceq::rt {

struct RpcError {
  enum class ErrorType {
    NetworkError,
    ConnectionRefused,
    HandlerNotFound,
    Internal,
    ParseError,
    Cancelled,
  };

  explicit RpcError(ErrorType error_type = ErrorType::Internal,
                    const std::string& message = "") noexcept;

  std::string Message() noexcept;

  ErrorType error_type;
  std::string status_message;
};

}  // namespace ceq::rt
