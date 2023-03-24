#pragma once

#include <vector>

#include <runtime/rpc_server.h>

namespace ceq::raft {

struct StartConfig {
  rt::Port port;
  std::vector<rt::Endpoint> cluster;
};

void RunMain(StartConfig config) noexcept;

}  // namespace ceq::raft
