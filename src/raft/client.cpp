#include "client.h"

namespace ceq::raft {

RaftClient::RaftClient(const Cluster& cluster) noexcept : cluster_{cluster} {
  for (const rt::rpc::Endpoint& ep : cluster) {
    clients_.emplace_back(ep);
  }
}

Result<Response, rt::rpc::Error> RaftClient::Execute(const RsmCommand& input, rt::Duration timeout,
                                                     size_t retry_count,
                                                     rt::StopToken stop_token) noexcept {
  LOG("EXECUTE: command = {}", input.data());

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

  rt::rpc::Error last_error;

  for (size_t attempt_id = 0; attempt_id < retry_count; ++retry_count) {
    LOG("EXECUTE: start attempt {} to {}", attempt_id, cluster_[current_leader_].address);
    auto& client = clients_[current_leader_];
    auto result = client.Execute(input, stop_source.GetToken());
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
      continue;
    }

    LOG("EXECUTE: attempt {} finished with success", attempt_id);

    return result;
  }

  if (stop_source.StopRequested()) {
    LOG("EXECUTE: stop requested");
    return Err(rt::rpc::Error::ErrorType::Cancelled);
  }

  LOG("EXECUTE: retry limit exceeded");
  return Err(std::move(last_error));
}

}  // namespace ceq::raft
