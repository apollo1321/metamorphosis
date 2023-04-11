#pragma once

#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <runtime/rpc_server.h>
#include <boost/fiber/buffered_channel.hpp>

#include "rpc_service_base.h"

namespace ceq::rt::rpc {

class Server::ServerImpl {
 public:
  void Register(Server::Service* service) noexcept;

  void Run(Port port, const ServerRunConfig& config) noexcept;

  void ShutDown() noexcept;

  ~ServerImpl();

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

  std::optional<boost::fibers::buffered_channel<Server::Service::RpcCallBase*>> channel_;

  std::vector<Server::Service*> services_;

  std::atomic<bool> running_ = false;
  std::atomic<bool> finished_ = false;
};

}  // namespace ceq::rt::rpc
