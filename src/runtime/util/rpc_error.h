#pragma once

#include <string>

namespace ceq::rt::rpc {

struct Error {
  enum class ErrorType {
    NetworkError,
    ConnectionRefused,
    HandlerNotFound,
    Internal,
    ParseError,
    Cancelled,
  };

  explicit Error(ErrorType error_type = ErrorType::Internal,
                 const std::string& message = "") noexcept;

  std::string Message() const noexcept;

  ErrorType error_type;
  std::string status_message;
};

}  // namespace ceq::rt::rpc
