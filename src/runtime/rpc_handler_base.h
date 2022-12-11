#pragma once

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

class RpcHandlerBase : public ::grpc::Service {
 public:
  struct RunConfig {
    size_t queue_count{};
    size_t threads_per_queue{};
    size_t worker_threads_count{};
  };

 public:
  void Run(const std::string& address, const RunConfig& config = MakeDefaultRunConfig());
  void ShutDown();
  bool IsRunning() const noexcept;

  static RunConfig MakeDefaultRunConfig() noexcept;

 protected:
  struct RpcCallBase : public FiberTask {
    virtual void PutNewCallInQueue(size_t queue_id) noexcept = 0;

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
  virtual void PutAllMethodsCallsInQueue(size_t queue_id) = 0;

  template <class RpcCall>
  void PutRpcCallInQueue(int method_id, int queue_id, RpcCall* rpc_call) {
    RequestAsyncUnary(method_id, &rpc_call->context, &rpc_call->request, &rpc_call->responder,
                      queues_[queue_id].get(), queues_[queue_id].get(), rpc_call);
  }

  static grpc::Status SyncMethodStub() {
    BOOST_ASSERT_MSG(false, "sync version of method must not be called");
    BOOST_UNREACHABLE_RETURN();
  }

 private:
  void RunDispatchingWorker(size_t queue_id) noexcept;
  void RunWorker() noexcept;

 private:
  std::unique_ptr<grpc::Server> server_;

  std::vector<std::thread> worker_threads_;
  std::vector<std::thread> dispatching_threads_;
  std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> queues_;

  std::unique_ptr<FiberTaskChannel> channel_;

  std::atomic<bool> running_ = false;
};
