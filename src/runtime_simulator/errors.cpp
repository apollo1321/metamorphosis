#include "errors.h"

#include <util/condition_check.h>

namespace runtime_simulation {
RpcError::RpcError(ErrorType error_type, const std::string& message) noexcept
    : error_type{error_type}, status_message{std::move(message)} {
}

std::string RpcError::Message() noexcept {
  std::string result = [&] {
    switch (error_type) {
      case ErrorType::HostNotFound:
        return "HostNotFound";
      case ErrorType::NetworkError:
        return "NetworkError";
      case ErrorType::ConnectionRefused:
        return "ConnectionRefused";
      case ErrorType::ServiceNotFound:
        return "ServiceNotFound";
      case ErrorType::HandlerNotFound:
        return "HandlerNotFound";
      case ErrorType::Internal:
        return "Internal";
      case ErrorType::ParseError:
        return "ParseError";
      default:
        VERIFY(false, "invalid error type");
    }
  }();
  if (!status_message.empty()) {
    result += ": " + status_message;
  }
  return result;
}
}  // namespace runtime_simulation
