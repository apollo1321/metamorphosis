#pragma once

#include <memory>
#include <vector>

#include <runtime/api.h>
#include <runtime/util/proto/proto_conversion.h>

#include <raft/raft.client.h>

namespace ceq::raft {

class RaftClient {
 public:
  explicit RaftClient(const std::vector<rt::Endpoint>& raft_nodes) noexcept;

  template <class ProtoOut, class ProtoIn>
  Result<ProtoOut, rt::rpc::Error> Apply(const ProtoIn& command, rt::Duration timeout,
                                         size_t retry_count,
                                         rt::StopToken stop_token = {}) noexcept {
    auto any_command = rt::proto::ToAny(command);
    if (any_command.HasError()) {
      return Err(rt::rpc::Error::ErrorType::Internal, std::move(any_command.GetError()));
    }

    auto any_result = Apply(any_command.GetValue(), timeout, retry_count, std::move(stop_token));
    if (any_result.HasError()) {
      return Err(std::move(any_result.GetError()));
    }

    auto result = rt::proto::FromAny<ProtoOut>(any_result.GetValue());

    if (result.HasError()) {
      return Err(rt::rpc::Error::ErrorType::Internal, std::move(result.GetError()));
    }

    return Ok(std::move(result.GetValue()));
  }

 private:
  Result<google::protobuf::Any, rt::rpc::Error> Apply(const google::protobuf::Any& command,
                                                      rt::Duration timeout, size_t retry_count,
                                                      rt::StopToken stop_token = {}) noexcept;

 private:
  std::vector<rt::Endpoint> raft_nodes_;
  std::vector<std::unique_ptr<rt::rpc::RaftApiClient>> clients_;

  size_t current_leader_ = 0;
  uint64_t client_id_ = 0;
};

}  // namespace ceq::raft
