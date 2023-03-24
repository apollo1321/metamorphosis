#include "rpc_server.h"

#include <boost/fiber/fiber.hpp>

#include <util/condition_check.h>

namespace ceq::rt {

void RpcServer::RpcServerImpl::Register(RpcServer::RpcService* service) noexcept {
  VERIFY(!running_, "cannot register service in running server");
  services_.emplace_back(service);
}

void RpcServer::RpcServerImpl::Run(Port port, const ServerRunConfig& config) noexcept {
  VERIFY(!running_.exchange(true), "server is running already");
  VERIFY(!finished_, "server cannot be run twice");

  channel_.emplace(kQueueSize);

  grpc::ServerBuilder builder;

  for (auto& service : services_) {
    builder.RegisterService(service);
  }
  builder.AddListeningPort("127.0.0.1:" + std::to_string(port), grpc::InsecureServerCredentials());

  for (size_t i = 0; i < config.queue_count; ++i) {
    queues_.emplace_back(builder.AddCompletionQueue());
  }

  server_ = builder.BuildAndStart();

  const size_t dispatching_threads_count = config.threads_per_queue * config.queue_count;
  dispatching_threads_.clear();
  dispatching_threads_.reserve(dispatching_threads_count);
  for (size_t thread_id = 0; thread_id < dispatching_threads_count; ++thread_id) {
    dispatching_threads_.emplace_back([this, queue_id = thread_id / config.threads_per_queue]() {
      RunDispatchingWorker(*queues_[queue_id]);
    });
  }

  worker_threads_.reserve(config.worker_threads_count);
  for (size_t thread_id = 0; thread_id < config.worker_threads_count; ++thread_id) {
    worker_threads_.emplace_back([this]() {
      RunWorker();
    });
  }
}

void RpcServer::RpcServerImpl::RunDispatchingWorker(grpc::ServerCompletionQueue& queue) noexcept {
  for (auto& service : services_) {
    service->PutAllMethodsCallsInQueue(queue);
  }

  void* tag = nullptr;
  bool ok = false;
  while (queue.Next(&tag, &ok)) {
    auto rpc_call = static_cast<RpcServer::RpcService::RpcCallBase*>(tag);
    if (rpc_call->finished || !ok) {
      delete rpc_call;
    } else {
      rpc_call->PutNewCallInQueue(queue);
      channel_->push(rpc_call);
    }
  }
}

void RpcServer::RpcServerImpl::RunWorker() noexcept {
  while (!channel_->is_closed()) {
    using boost::fibers::channel_op_status;
    using boost::fibers::launch;

    RpcServer::RpcService::RpcCallBase* task;
    auto status = channel_->pop(task);
    if (status != channel_op_status::success) {
      break;
    }

    boost::fibers::fiber(launch::dispatch, std::ref(*task)).detach();
  }
}

void RpcServer::RpcServerImpl::ShutDown() noexcept {
  VERIFY(running_.exchange(false) && !finished_.exchange(true), "server is not running");

  server_->Shutdown();
  for (auto& queue : queues_) {
    queue->Shutdown();
  }
  channel_->close();
  for (auto& thread : worker_threads_) {
    thread.join();
  }
  for (auto& thread : dispatching_threads_) {
    thread.join();
  }
}

}  // namespace ceq::rt
