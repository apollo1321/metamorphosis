#pragma once

#include <utility>
#include <vector>

#include <google/protobuf/any.pb.h>

#include <runtime/api.h>
#include <runtime/rpc_server.h>

namespace ceq::raft {

struct IStateMachine {
  virtual google::protobuf::Any Apply(const google::protobuf::Any& command) noexcept = 0;
  virtual ~IStateMachine() = default;
};

struct RaftConfig {
  size_t node_id{};
  std::vector<rt::Endpoint> raft_nodes;

  rt::Interval election_timeout;
  rt::Duration heart_beat_period;
  rt::Duration rpc_timeout;

  std::filesystem::path log_db_path;
  std::filesystem::path raft_state_db_path;
};

void RunMain(IStateMachine* state_machine, RaftConfig config) noexcept;

}  // namespace ceq::raft
