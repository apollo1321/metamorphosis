#include "rpc_error.h"

#include <util/condition_check.h>

namespace mtf::rt::rpc {

RpcError::RpcError(RpcErrorType error_type, const std::string& message) noexcept
    : error_type{error_type}, status_message{std::move(message)} {
}

std::string RpcError::Message() const noexcept {
  std::string result = [&] {
    switch (error_type) {
      case RpcErrorType::NetworkError:
        return "NetworkError";
      case RpcErrorType::ConnectionRefused:
        return "ConnectionRefused";
      case RpcErrorType::HandlerNotFound:
        return "HandlerNotFound";
      case RpcErrorType::Internal:
        return "Internal";
      case RpcErrorType::ParseError:
        return "ParseError";
      case RpcErrorType::Cancelled:
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

}  // namespace mtf::rt::rpc
