#pragma once

#include <vector>

#include <runtime/api.h>
#include <runtime/rpc_server.h>

namespace ceq::raft {

struct RaftConfig {
  rt::Port port;
  std::vector<rt::Endpoint> cluster;

  rt::Duration election_timeout;
};

void RunMain(RaftConfig config) noexcept;

}  // namespace ceq::raft
