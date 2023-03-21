#pragma once

#include <memory>
#include <string>
#include <thread>

#include <boost/fiber/buffered_channel.hpp>

#include <grpcpp/grpcpp.h>

#include "rpc_service_base.h"

namespace runtime {

struct RunConfig {
  size_t queue_count{};
  size_t threads_per_queue{};
  size_t worker_threads_count{};

  static RunConfig MakeDefaultRunConfig() noexcept;
};

using FiberTaskChannel = boost::fibers::buffered_channel<FiberTask*>;

class RpcServer {
 public:
  void Register(RpcServiceBase* service) noexcept;

  void Run(const std::string& address,
           const RunConfig& config = RunConfig::MakeDefaultRunConfig()) noexcept;

  void ShutDown() noexcept;

  bool IsRunning() const noexcept;

 private:
  static constexpr size_t kQueueSize = 1 << 10;

 private:
  void RunDispatchingWorker(grpc::ServerCompletionQueue& queue) noexcept;
  void RunWorker() noexcept;

 private:
  std::unique_ptr<grpc::Server> server_;

  std::vector<std::thread> worker_threads_;
  std::vector<std::thread> dispatching_threads_;
  std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> queues_;

  std::optional<FiberTaskChannel> channel_;

  std::vector<RpcServiceBase*> services_;

  std::atomic<bool> running_ = false;
  std::atomic<bool> finished_ = false;
};

}  // namespace runtime
