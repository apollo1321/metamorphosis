#pragma once

#include <memory>

#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/config_protobuf.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/support/method_handler.h>

#include "task.h"

namespace runtime {

class RpcServiceBase : public grpc::Service {
 protected:
  struct RpcCallBase : public FiberTask {
    virtual void PutNewCallInQueue(grpc::ServerCompletionQueue& queue) noexcept = 0;

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
  virtual void PutAllMethodsCallsInQueue(grpc::ServerCompletionQueue& queue) = 0;

  template <class RpcCall>
  void PutRpcCallInQueue(grpc::ServerCompletionQueue& queue, int method_id, RpcCall* rpc_call) {
    RequestAsyncUnary(method_id, &rpc_call->context, &rpc_call->request, &rpc_call->responder,
                      &queue, &queue, rpc_call);
  }

  static grpc::Status SyncMethodStub();

  friend class RpcServer;
};

}  // namespace runtime