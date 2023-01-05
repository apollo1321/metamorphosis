#include "rpc_handler_base.h"

#include <util/condition_check.h>
#include <boost/fiber/fiber.hpp>

void RpcHandlerBase::Run(const std::string& address, const RunConfig& config) {
  if (running_.exchange(true)) {
    throw std::runtime_error("server is running already");
  }

  constexpr size_t kQueueSize = 1 << 10;
  channel_ = std::make_unique<FiberTaskChannel>(kQueueSize);

  grpc::ServerBuilder builder;

  builder.RegisterService(this);
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());

  queues_.clear();
  queues_.resize(config.queue_count);
  for (auto& queue : queues_) {
    queue = builder.AddCompletionQueue();
  }
  server_ = builder.BuildAndStart();

  const size_t dispatching_threads_count = config.threads_per_queue * config.queue_count;
  dispatching_threads_.clear();
  dispatching_threads_.reserve(dispatching_threads_count);
  for (size_t thread_id = 0; thread_id < dispatching_threads_count; ++thread_id) {
    dispatching_threads_.emplace_back([this, queue_id = thread_id / config.threads_per_queue]() {
      RunDispatchingWorker(queue_id);
    });
  }

  worker_threads_.clear();
  worker_threads_.reserve(config.worker_threads_count);
  for (size_t thread_id = 0; thread_id < config.worker_threads_count; ++thread_id) {
    worker_threads_.emplace_back([this]() {
      RunWorker();
    });
  }
}

bool RpcHandlerBase::IsRunning() const noexcept {
  return running_;
}

void RpcHandlerBase::RunDispatchingWorker(size_t queue_id) noexcept {
  PutAllMethodsCallsInQueue(queue_id);

  void* tag = nullptr;
  bool ok = false;
  while (queues_[queue_id]->Next(&tag, &ok)) {
    auto rpc_call = static_cast<RpcCallBase*>(tag);
    if (rpc_call->finished || !ok) {
      delete rpc_call;
    } else {
      rpc_call->PutNewCallInQueue(queue_id);
      channel_->push(rpc_call);
    }
  }
}

void RpcHandlerBase::RunWorker() noexcept {
  while (!channel_->is_closed()) {
    using boost::fibers::channel_op_status;
    using boost::fibers::launch;

    FiberTask* task;
    auto status = channel_->pop(task);
    if (status != channel_op_status::success) {
      break;
    }

    boost::fibers::fiber(launch::dispatch, std::ref(*task)).detach();
  }
}

void RpcHandlerBase::ShutDown() {
  if (!running_.exchange(false)) {
    throw std::runtime_error("server is not running");
  }
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

RpcHandlerBase::RunConfig RpcHandlerBase::MakeDefaultRunConfig() noexcept {
  auto queue_count = std::max(std::thread::hardware_concurrency() / 2, 1u);
  auto worker_count = std::max(std::thread::hardware_concurrency() / 4, 1u);
  return {.queue_count = queue_count, .threads_per_queue = 2, .worker_threads_count = worker_count};
}

grpc::Status RpcHandlerBase::SyncMethodStub() {
  VERIFY(false, "sync version of method must not be called");
  abort();
}
