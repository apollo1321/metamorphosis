#pragma once

#include <runtime/api.h>
#include <util/defer.h>

#include <raft/raft.client.h>

#include "node.h"

namespace ceq::raft {

class RaftClient {
 public:
  explicit RaftClient(const std::vector<rt::rpc::Endpoint>& raft_nodes) noexcept;

  Result<google::protobuf::Any, rt::rpc::Error> Apply(const google::protobuf::Any& command,
                                                      rt::Duration timeout, size_t retry_count,
                                                      rt::StopToken stop_token = {}) noexcept;

 private:
  std::vector<rt::rpc::Endpoint> raft_nodes_;
  std::vector<rt::rpc::RaftApiClient> clients_;

  size_t current_leader_ = 0;
  uint64_t client_id_ = 0;
};

}  // namespace ceq::raft
