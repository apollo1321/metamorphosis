#include "rpc_handler_base.h"

#include <barrier>

void RpcHandlerBase::Run(const std::string& address, FiberTaskChannel* channel,
                         size_t thread_count) {
  if (running_.exchange(true)) {
    throw std::runtime_error("server is running already");
  }

  channel_ = channel;

  grpc::ServerBuilder builder;

  builder.RegisterService(this);
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  queue_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();

  threads_.clear();
  threads_.reserve(thread_count);
  for (size_t thread_id = 0; thread_id < thread_count; ++thread_id) {
    threads_.emplace_back([this]() {
      RunWorker();
    });
  }
}

bool RpcHandlerBase::IsRunning() const noexcept {
  return running_;
}

void RpcHandlerBase::RunWorker() noexcept {
  PutAllMethodsCallsInQueue();

  void* tag = nullptr;
  bool ok = false;
  while (queue_->Next(&tag, &ok)) {
    auto rpc_call = static_cast<RpcCallBase*>(tag);
    if (rpc_call->finished) {
      delete rpc_call;
    } else {
      rpc_call->PutNewCallInQueue();
      channel_->push(rpc_call);
    }
  }
}

void RpcHandlerBase::ShutDown() {
  if (!running_.exchange(false)) {
    throw std::runtime_error("server is not running");
  }
  server_->Shutdown();
  queue_->Shutdown();
  for (auto& thread : threads_) {
    thread.join();
  }
}
