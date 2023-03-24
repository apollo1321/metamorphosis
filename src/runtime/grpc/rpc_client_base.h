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

namespace ceq::rt {

class RpcClientBase {
 public:
  explicit RpcClientBase(const std::string& address) noexcept;

  ~RpcClientBase();

 protected:
  template <class Request, class Response>
  Response MethodImpl(const Request& request, const char* method_name) {
    using namespace grpc;            // NOLINT
    using namespace grpc::internal;  // NOLINT

    EventTrigger call;

    RpcMethod method{method_name, RpcMethod::RpcType::NORMAL_RPC};
    ClientContext context;
    Status status;
    Response reply;

    std::unique_ptr<ClientAsyncResponseReader<Response>> response_reader(
        ClientAsyncResponseReaderHelper::Create<Response, Request>(channel_.get(), &queue_, method,
                                                                   &context, request));

    response_reader->StartCall();
    response_reader->Finish(&reply, &status, static_cast<void*>(&call));

    std::unique_lock guard(call.mutex);
    call.is_ready.wait(guard, [&call]() {
      return call.is_set;
    });

    if (!status.ok()) {
      throw std::runtime_error("RPC Failed: " + status.error_message());
    }

    return std::move(reply);
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
