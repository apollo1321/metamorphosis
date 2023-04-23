#include "rpc_server.h"

#include <algorithm>
#include <exception>
#include <thread>

#include <sstream>

#ifdef SIMULATION
#include "simulator/rpc_server.h"
#else
#include "production/rpc_server.h"
#endif

namespace ceq::rt::rpc {


ServerRunConfig::ServerRunConfig() noexcept {
  queue_count = std::max(std::thread::hardware_concurrency() / 2, 1u);
  worker_threads_count = std::max(std::thread::hardware_concurrency() / 4, 1u);
  threads_per_queue = 2;
}

Server::Server() noexcept : impl_{new ServerImpl} {
}

void Server::Register(Service* service) noexcept {
  impl_->Register(service);
}

void Server::Run(Port port, ServerRunConfig run_config) noexcept {
  impl_->Run(port, run_config);
}

void Server::ShutDown() noexcept {
  impl_->ShutDown();
}

Server::~Server() {
  delete impl_;
}

}  // namespace ceq::rt::rpc
