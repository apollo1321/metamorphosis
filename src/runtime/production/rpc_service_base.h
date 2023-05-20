#pragma once

#include <runtime/rpc_server.h>
#include <runtime/util/latch/latch.h>
#include <runtime/util/rpc_error/rpc_error.h>

#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/config_protobuf.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/support/method_handler.h>

#include <memory>

namespace ceq::rt::rpc {

class Server::Service : public grpc::Service {
 protected:
  struct RpcCallBase {
    explicit RpcCallBase(LatchGuard guard) noexcept : guard{std::move(guard)} {
    }

    virtual void PutNewCallInQueue(grpc::ServerCompletionQueue& queue,
                                   LatchGuard guard) noexcept = 0;

    virtual void operator()() noexcept = 0;

    virtual ~RpcCallBase() = default;

    bool finished = false;

    LatchGuard guard;
  };

  template <class Request, class Response>
  struct RpcCall : public RpcCallBase {
    explicit RpcCall(LatchGuard guard) : RpcCallBase(std::move(guard)), responder(&context) {
    }

    template <class Handler>
    void RunImpl(Handler handler) noexcept {
      finished = true;

      auto reply = handler(request);

      RpcError err;
      if (reply.HasValue()) {
        responder.Finish(reply.GetValue(), grpc::Status(), this);
      } else {
        responder.FinishWithError(
            grpc::Status(grpc::StatusCode::INTERNAL, reply.GetError().status_message), this);
      }
    }

    grpc::ServerContext context;
    grpc::ServerAsyncResponseWriter<Response> responder;

    Request request;
  };

 protected:
  virtual void PutAllMethodsCallsInQueue(grpc::ServerCompletionQueue& queue, Latch& latch) = 0;

  template <class RpcCall>
  void PutRpcCallInQueue(grpc::ServerCompletionQueue& queue, int method_id, RpcCall* rpc_call) {
    RequestAsyncUnary(method_id, &rpc_call->context, &rpc_call->request, &rpc_call->responder,
                      &queue, &queue, rpc_call);
  }

  static grpc::Status SyncMethodStub();

  friend class Server::ServerImpl;
};

}  // namespace ceq::rt::rpc
