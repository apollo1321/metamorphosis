#pragma once

#include <vector>

#include <common/defs.h>

namespace ceq::raft {

struct StartConfig {
  std::vector<Endpoint> cluster;
};

void RunMain(const StartConfig& config) noexcept;

}  // namespace ceq::raft
