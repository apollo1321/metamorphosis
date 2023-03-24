#include "rpc_server.h"

#include <algorithm>
#include <thread>

#ifdef RT_SIMULATION
#include "simulator/rpc_server.h"
#else
#include "grpc/rpc_server.h"
#endif

namespace ceq::rt {

ServerRunConfig::ServerRunConfig() noexcept {
  queue_count = std::max(std::thread::hardware_concurrency() / 2, 1u);
  worker_threads_count = std::max(std::thread::hardware_concurrency() / 4, 1u);
  threads_per_queue = 2;
}

RpcServer::RpcServer() noexcept : impl_{new RpcServerImpl} {
}

void RpcServer::Register(RpcService* service) noexcept {
  impl_->Register(service);
}

void RpcServer::Run(Port port, ServerRunConfig run_config) noexcept {
  impl_->Run(port, run_config);
}

void RpcServer::ShutDown() noexcept {
  impl_->ShutDown();
}

RpcServer::~RpcServer() {
  delete impl_;
}

}  // namespace ceq::rt
