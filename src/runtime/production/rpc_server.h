#pragma once

#include "rpc_service_base.h"

#include <runtime/rpc_server.h>
#include <runtime/util/latch/latch.h>

#include <grpcpp/grpcpp.h>
#include <boost/fiber/buffered_channel.hpp>

#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace ceq::rt::rpc {

class Server::ServerImpl {
 public:
  void Register(Server::Service* service) noexcept;

  void Start(Port port) noexcept;

  void Run() noexcept;

  void ShutDown() noexcept;

  ~ServerImpl();

 private:
  static constexpr size_t kQueueSize = 1 << 10;

 private:
  void RunDispatchingWorker() noexcept;

 private:
  std::unique_ptr<grpc::Server> server_;

  std::thread dispatching_thread_;
  std::unique_ptr<grpc::ServerCompletionQueue> queue_;

  boost::fibers::buffered_channel<Server::Service::RpcCallBase*> channel_{kQueueSize};

  std::vector<Server::Service*> services_;

  std::atomic<bool> running_ = false;
  std::atomic<bool> finished_ = false;

  Latch requests_latch_;
  Latch workers_latch_;
};

}  // namespace ceq::rt::rpc
