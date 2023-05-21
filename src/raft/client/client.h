#pragma once

#include <memory>
#include <vector>

#include <runtime/api.h>
#include <runtime/util/backoff/backoff.h>
#include <runtime/util/proto/proto_conversion.h>

#include <raft/raft.client.h>

namespace ceq::raft {

enum class RaftClientErrorType {
  GlobalTimeout,
  RpcTimeout,
  NotLeader,
};

struct RaftClientError {
  explicit RaftClientError(rt::rpc::RpcError error) noexcept;
  explicit RaftClientError(RaftClientErrorType error_type) noexcept;

  std::string Message() const noexcept;

  std::variant<RaftClientErrorType, rt::rpc::RpcError> error;
};

class RaftClient {
 public:
  struct Config {
    Config(rt::Duration global_timeout, rt::Duration rpc_timeout, size_t retry_count,
           rt::BackoffParams backoff_params = {}) noexcept;

    rt::Duration global_timeout;
    rt::Duration rpc_timeout;
    size_t retry_count;
    rt::BackoffParams backoff_params;
  };

 public:
  explicit RaftClient(const std::vector<rt::Endpoint>& raft_nodes) noexcept;

  template <class ProtoOut, class ProtoIn>
  Result<ProtoOut, RaftClientError> Apply(const ProtoIn& command, const Config& config,
                                          rt::StopToken stop_token = {}) noexcept {
    using namespace rt::rpc;  // NOLINT
    LOG("RAFT_CLIENT: start with command: {}", command);

    return rt::proto::ToAny(command)
        .TransformError([](std::string&& error) {
          return RaftClientError(RpcError(RpcErrorType::ParseError, std::move(error)));
        })
        .AndThen([&](auto&& any_command) {
          return Apply(any_command, config, std::move(stop_token));
        })
        .AndThen([&](auto&& any_response) {
          return rt::proto::FromAny<ProtoOut>(any_response).TransformError([](std::string&& error) {
            return RaftClientError(RpcError(RpcErrorType::ParseError, std::move(error)));
          });
        });
  }

 private:
  Result<google::protobuf::Any, RaftClientError> Apply(const google::protobuf::Any& command,
                                                       const Config& config,
                                                       rt::StopToken stop_token) noexcept;

  Result<google::protobuf::Any, RaftClientError> StartAttempt(const Request& request,
                                                              rt::Duration rpc_timeout,
                                                              rt::StopToken stop_token);

 private:
  std::vector<rt::Endpoint> raft_nodes_;
  std::vector<std::unique_ptr<rt::rpc::RaftApiClient>> clients_;

  size_t current_leader_ = 0;
  uint64_t client_id_ = 0;
};

}  // namespace ceq::raft
