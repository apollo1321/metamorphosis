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
      : status_message{std::move(message)} {
  }

  std::string status_message;
  ErrorType error_type;
};

}  // namespace runtime_simulation
