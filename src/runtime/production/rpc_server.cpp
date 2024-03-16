#include "rpc_server.h"

#include <boost/fiber/all.hpp>

#include <util/condition_check.h>

namespace mtf::rt::rpc {

void Server::ServerImpl::Register(Server::Service* service) noexcept {
  VERIFY(!running_, "cannot register service in running server");
  services_.emplace_back(service);
}

void Server::ServerImpl::Start(Port port) noexcept {
  VERIFY(!running_.exchange(true), "server is running already");
  VERIFY(!finished_, "server cannot be run twice");

  grpc::ServerBuilder builder;

  for (auto& service : services_) {
    builder.RegisterService(service);
  }
  builder.AddListeningPort("[::]:" + std::to_string(port), grpc::InsecureServerCredentials());

  queue_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();

  dispatching_thread_ = std::thread([this]() {
    RunDispatchingWorker();
  });
}

void Server::ServerImpl::Run() noexcept {
  auto guard = workers_latch_.MakeGuard();
  while (!channel_.is_closed()) {
    using boost::fibers::channel_op_status;
    using boost::fibers::launch;

    Server::Service::RpcCallBase* task{};
    auto status = channel_.pop(task);
    if (status != channel_op_status::success) {
      break;
    }

    boost::fibers::fiber(launch::post, std::ref(*task)).detach();
  }
}

void Server::ServerImpl::RunDispatchingWorker() noexcept {
  for (auto& service : services_) {
    service->PutAllMethodsCallsInQueue(*queue_, requests_latch_);
  }

  void* tag = nullptr;
  bool ok = false;
  while (queue_->Next(&tag, &ok)) {
    auto rpc_call = static_cast<Server::Service::RpcCallBase*>(tag);
    if (rpc_call->finished || !ok || !running_) {
      delete rpc_call;
    } else {
      rpc_call->PutNewCallInQueue(*queue_, requests_latch_.MakeGuard());
      channel_.push(rpc_call);
    }
  }
}

void Server::ServerImpl::ShutDown() noexcept {
  VERIFY(running_.exchange(false) && !finished_.exchange(true), "server is not running");

  server_->Shutdown();
  requests_latch_.AwaitZero();
  queue_->Shutdown();
  channel_.close();

  dispatching_thread_.join();
  workers_latch_.AwaitZero();
}

Server::ServerImpl::~ServerImpl() {
  VERIFY(finished_, "Destruction of running server");
}

}  // namespace mtf::rt::rpc
