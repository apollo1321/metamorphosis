#pragma once

#include <memory>

#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/config_protobuf.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/support/method_handler.h>

#include <runtime/rpc_server.h>
#include <runtime/util/rpc_error/rpc_error.h>

namespace ceq::rt::rpc {

class Server::Service : public grpc::Service {
 protected:
  struct RpcCallBase {
    virtual void PutNewCallInQueue(grpc::ServerCompletionQueue& queue) noexcept = 0;

    virtual void operator()() noexcept = 0;

    virtual ~RpcCallBase() = default;

    bool finished = false;
  };

  template <class Request, class Response>
  struct RpcCall : public RpcCallBase {
    RpcCall() : responder(&context) {
    }

    template <class Handler>
    void RunImpl(Handler handler) noexcept {
      finished = true;

      auto reply = handler(request);

      Error err;
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
  virtual void PutAllMethodsCallsInQueue(grpc::ServerCompletionQueue& queue) = 0;

  template <class RpcCall>
  void PutRpcCallInQueue(grpc::ServerCompletionQueue& queue, int method_id, RpcCall* rpc_call) {
    RequestAsyncUnary(method_id, &rpc_call->context, &rpc_call->request, &rpc_call->responder,
                      &queue, &queue, rpc_call);
  }

  static grpc::Status SyncMethodStub();

  friend class Server::ServerImpl;
};

}  // namespace ceq::rt::rpc
