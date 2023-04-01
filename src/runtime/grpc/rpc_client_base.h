#pragma once

#include <memory>
#include <thread>

#include <grpcpp/completion_queue.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/impl/rpc_method.h>
#include <grpcpp/support/async_unary_call.h>

#include <boost/fiber/condition_variable.hpp>
#include <boost/fiber/mutex.hpp>

#include <runtime/cancellation/stop_callback.h>
#include <runtime/cancellation/stop_token.h>
#include <runtime/rpc_error.h>
#include <util/result.h>

namespace ceq::rt {

class RpcClientBase {
 public:
  explicit RpcClientBase(const std::string& address) noexcept;

  ~RpcClientBase();

 protected:
  template <class Request, class Response>
  Result<Response, RpcError> MakeRequest(const Request& request, const char* method_name,
                                         StopToken stop_token = {}) noexcept {
    using namespace grpc;            // NOLINT
    using namespace grpc::internal;  // NOLINT

    EventTrigger call;

    RpcMethod method{method_name, RpcMethod::RpcType::NORMAL_RPC};
    ClientContext context;
    grpc::Status status;
    Response reply;

    std::unique_ptr<ClientAsyncResponseReader<Response>> response_reader(
        ClientAsyncResponseReaderHelper::Create<Response, Request>(channel_.get(), &queue_, method,
                                                                   &context, request));

    response_reader->StartCall();
    response_reader->Finish(&reply, &status, static_cast<void*>(&call));

    StopCallback stop_callback(stop_token, [&]() {
      context.TryCancel();
    });

    std::unique_lock guard(call.mutex);
    call.is_ready.wait(guard, [&call]() {
      return call.is_set;
    });

    if (!status.ok()) {
      if (status.error_message().find("Connection refused") != std::string::npos) {
        return Err(RpcError::ErrorType::ConnectionRefused, status.error_message());
      }
      switch (status.error_code()) {
        case grpc::CANCELLED:
          return Err(RpcError::ErrorType::Cancelled, status.error_message());
        case grpc::UNIMPLEMENTED:
          return Err(RpcError::ErrorType::HandlerNotFound, status.error_message());
        default:
          return Err(RpcError::ErrorType::Internal, status.error_message());
      }
    }

    return Ok(std::move(reply));
  }

 private:
  struct EventTrigger {
    bool is_set{false};
    boost::fibers::condition_variable is_ready;
    boost::fibers::mutex mutex;
  };

 private:
  void DispatchServiceResponses();

 private:
  grpc::CompletionQueue queue_;
  std::shared_ptr<grpc::Channel> channel_;
  std::thread dispatching_thread_;
};

}  // namespace ceq::rt
