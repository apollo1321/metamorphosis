#include "rpc_error.h"

#include <util/condition_check.h>

namespace ceq::rt::rpc {

Error::Error(ErrorType error_type, const std::string& message) noexcept
    : error_type{error_type}, status_message{std::move(message)} {
}

std::string Error::Message() const noexcept {
  std::string result = [&] {
    switch (error_type) {
      case ErrorType::NetworkError:
        return "NetworkError";
      case ErrorType::ConnectionRefused:
        return "ConnectionRefused";
      case ErrorType::HandlerNotFound:
        return "HandlerNotFound";
      case ErrorType::Internal:
        return "Internal";
      case ErrorType::ParseError:
        return "ParseError";
      case ErrorType::Cancelled:
        return "Cancelled";
      default:
        VERIFY(false, "invalid error type");
    }
  }();
  if (!status_message.empty()) {
    result += ": " + status_message;
  }
  return result;
}

}  // namespace ceq::rt::rpc
