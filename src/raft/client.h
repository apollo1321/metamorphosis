#pragma once

#include <runtime/api.h>
#include <util/defer.h>

#include <raft/raft.client.h>
#include <raft/util.h>

namespace ceq::raft {

class RaftClient {
 public:
  explicit RaftClient(const Cluster& cluster) noexcept;

  Result<Response, rt::rpc::Error> Execute(const RsmCommand& input, rt::Duration timeout,
                                           size_t retry_count,
                                           rt::StopToken stop_token = {}) noexcept;

 private:
  Cluster cluster_;
  std::vector<rt::rpc::RaftApiClient> clients_;

  size_t current_leader_ = 0;
};

}  // namespace ceq::raft
