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

Server::Server() noexcept : impl_{new ServerImpl} {
}

void Server::Register(Service* service) noexcept {
  impl_->Register(service);
}

// Start accepting requests
void Server::Start(Port port) noexcept {
  impl_->Start(port);
}

// Start processing service handlers, may be called from different threads
void Server::Run() noexcept {
  impl_->Run();
}

void Server::ShutDown() noexcept {
  impl_->ShutDown();
}

Server::~Server() {
  delete impl_;
}

}  // namespace ceq::rt::rpc
