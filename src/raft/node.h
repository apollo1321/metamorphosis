#pragma once

#include <utility>
#include <vector>

#include <runtime/api.h>
#include <runtime/rpc_server.h>

namespace ceq::raft {

struct RaftConfig {
  size_t node_id{};
  std::vector<rt::Endpoint> cluster;

  std::pair<rt::Duration, rt::Duration> election_timeout_interval;
  rt::Duration heart_beat_period;
  rt::Duration rpc_timeout;
};

void RunMain(RaftConfig config) noexcept;

}  // namespace ceq::raft
