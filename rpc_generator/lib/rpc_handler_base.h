#pragma once

#include <barrier>
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/config_protobuf.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/support/method_handler.h>

#include <boost/assert.hpp>
#include <boost/fiber/buffered_channel.hpp>

class FiberTask {
 public:
  virtual void operator()() noexcept = 0;

  virtual ~FiberTask() = default;
};

using FiberTaskChannel = boost::fibers::buffered_channel<FiberTask*>;

class RpcHandlerBase : protected ::grpc::Service {
 public:
  void Run(const std::string& address, FiberTaskChannel* channel, size_t thread_count = 1);
  void ShutDown();
  bool IsRunning() const noexcept;

 protected:
  struct RpcCallBase : public FiberTask {
    virtual void PutNewCallInQueue() noexcept = 0;

    bool finished = false;
  };

  template <class Request, class Response>
  struct RpcCall : public RpcCallBase {
    RpcCall() : responder(&context) {
    }

    template <class Handler>
    void RunImpl(Handler handler) noexcept {
      finished = true;

      try {
        auto reply = handler(request);
        responder.Finish(reply, grpc::Status(), this);
      } catch (std::exception& e) {
        responder.FinishWithError(grpc::Status(grpc::StatusCode::INTERNAL,
                                               std::string("exception is thrown: ") + e.what()),
                                  this);
      } catch (...) {
        responder.FinishWithError(
            grpc::Status(grpc::StatusCode::INTERNAL, std::string("unknown exception is thrown")),
            this);
      }
    }

    grpc::ServerContext context;
    grpc::ServerAsyncResponseWriter<Response> responder;

    Request request;
  };

 protected:
  virtual void PutAllMethodsCallsInQueue() = 0;

  template <class RpcCall>
  void PutRpcCallInQueue(int index, RpcCall* rpc_call) {
    RequestAsyncUnary(index, &rpc_call->context, &rpc_call->request, &rpc_call->responder,
                      queue_.get(), queue_.get(), rpc_call);
  }

  static grpc::Status SyncMethodStub() {
    BOOST_ASSERT_MSG(false, "sync version of method must not be called");
    BOOST_UNREACHABLE_RETURN();
  }

 private:
  void RunWorker() noexcept;

 private:
  std::unique_ptr<grpc::ServerCompletionQueue> queue_;
  std::unique_ptr<grpc::Server> server_;

  std::vector<std::thread> threads_;
  FiberTaskChannel* channel_;

  std::atomic<bool> running_ = false;
};
