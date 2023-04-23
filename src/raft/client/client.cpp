#include "client.h"

#include <util/defer.h>

namespace ceq::raft {

RaftClient::RaftClient(const std::vector<rt::Endpoint>& raft_nodes) noexcept
    : raft_nodes_{raft_nodes},
      client_id_{std::uniform_int_distribution<uint64_t>()(rt::GetGenerator())} {
  for (const rt::Endpoint& ep : raft_nodes_) {
    clients_.emplace_back(std::make_unique<rt::rpc::RaftApiClient>(ep));
  }
}

Result<google::protobuf::Any, rt::rpc::Error> RaftClient::Apply(
    const google::protobuf::Any& command, rt::Duration timeout, size_t retry_count,
    rt::StopToken stop_token) noexcept {
  LOG("EXECUTE: start");

  rt::StopSource stop_source;
  rt::StopCallback stop_propagate(stop_token, [&]() {
    stop_source.Stop();
  });

  boost::fibers::fiber timeout_fiber([&]() {
    rt::SleepFor(timeout, stop_source.GetToken());
    stop_source.Stop();
  });

  DEFER {
    stop_source.Stop();
    timeout_fiber.join();
  };

  auto set_next_leader = [&]() {
    current_leader_ = (current_leader_ + 1) % clients_.size();
  };

  Request request;
  *request.mutable_command() = command;
  request.set_client_id(client_id_);
  request.set_request_id(std::uniform_int_distribution<uint64_t>()(rt::GetGenerator()));

  rt::rpc::Error last_error;

  for (size_t attempt_id = 0; attempt_id < retry_count && !stop_source.StopRequested();
       ++attempt_id) {
    LOG("EXECUTE: start attempt {} to {}", attempt_id, raft_nodes_[current_leader_].address);
    auto& client = *clients_[current_leader_];
    auto result = client.Execute(request, stop_source.GetToken());
    if (result.HasError()) {
      LOG("EXECUTE: attempt {} finished with error: {}", attempt_id, result.GetError().Message());
      last_error = std::move(result.GetError());
      set_next_leader();
      continue;
    }

    auto& response = result.GetValue();
    if (response.status() == Response::Status::Response_Status_NotALeader) {
      LOG("EXECUTE: attempt {} finished, node is not leader", attempt_id);
      set_next_leader();
      last_error = rt::rpc::Error(rt::rpc::Error::ErrorType::Internal, "node is not a leader");
      continue;
    }

    LOG("EXECUTE: attempt {} finished with success", attempt_id);

    return Ok(result.GetValue().result());
  }

  if (stop_source.StopRequested()) {
    LOG("EXECUTE: stop requested");
    return Err(rt::rpc::Error::ErrorType::Cancelled);
  }

  LOG("EXECUTE: retry limit exceeded");
  return Err(std::move(last_error));
}

}  // namespace ceq::raft