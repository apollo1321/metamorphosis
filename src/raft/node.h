#pragma once

#include <utility>
#include <vector>

#include <google/protobuf/any.pb.h>

#include <runtime/api.h>
#include <runtime/rpc_server.h>

namespace ceq::raft {

using Cluster = std::vector<rt::rpc::Endpoint>;

struct IStateMachine {
  virtual google::protobuf::Any Apply(const google::protobuf::Any& command) noexcept = 0;
  virtual ~IStateMachine() = default;
};

struct RaftConfig {
  size_t node_id{};
  Cluster cluster;

  std::pair<rt::Duration, rt::Duration> election_timeout_interval;
  rt::Duration heart_beat_period;
  rt::Duration rpc_timeout;
};

void RunMain(IStateMachine* state_machine, RaftConfig config) noexcept;

}  // namespace ceq::raft
