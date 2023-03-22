#pragma once

#include <string>

namespace runtime_simulation {

struct RpcError {
  enum class ErrorType {
    HostNotFound,
    NetworkError,
    ConnectionRefused,
    ServiceNotFound,
    HandlerNotFound,
    Internal,
    ParseError,
  };

  explicit RpcError(ErrorType error_type = ErrorType::Internal,
                    const std::string& message = "") noexcept
      : error_type{error_type}, status_message{std::move(message)} {
  }

  ErrorType error_type;
  std::string status_message;
};

}  // namespace runtime_simulation
