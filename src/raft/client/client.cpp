#include "client.h"

#include <util/defer.h>
#include "runtime/util/print/print.h"

namespace ceq::raft {

RaftClientError::RaftClientError(rt::rpc::RpcError error) noexcept : error{std::move(error)} {
}

RaftClientError::RaftClientError(RaftClientErrorType error_type) noexcept
    : error{std::move(error_type)} {
}

std::string RaftClientError::Message() const noexcept {
  if (std::holds_alternative<rt::rpc::RpcError>(error)) {
    return std::get<rt::rpc::RpcError>(error).Message();
  }
  auto error_type = std::get<RaftClientErrorType>(error);
  switch (error_type) {
    case RaftClientErrorType::GlobalTimeout:
      return "GlobalTimeout";
    case RaftClientErrorType::RpcTimeout:
      return "RpcTimeout";
    case RaftClientErrorType::NotLeader:
      return "NotLeader";
  }
  VERIFY(false, "unexpected raft client error type");
}

RaftClient::Config::Config(rt::Duration global_timeout, rt::Duration rpc_timeout,
                           size_t retry_count, rt::BackoffParams backoff_params) noexcept
    : global_timeout{global_timeout},
      rpc_timeout{rpc_timeout},
      retry_count{retry_count},
      backoff_params(backoff_params) {
}

enum class RequestState {
  Running,
  Cancelled,
  GlobalTimeout,
  Finished,
};

RaftClient::RaftClient(const std::vector<rt::Endpoint>& raft_nodes) noexcept
    : raft_nodes_{raft_nodes}, client_id_{rt::GetRandomInt()} {
  for (const rt::Endpoint& ep : raft_nodes_) {
    clients_.emplace_back(std::make_unique<rt::rpc::RaftApiClient>(ep));
  }
}

Result<google::protobuf::Any, RaftClientError> RaftClient::Apply(
    const google::protobuf::Any& command, const Config& config, rt::StopToken stop_token) noexcept {
  VERIFY(config.rpc_timeout.count() > 0, "invalid rpc_timeout");
  VERIFY(config.global_timeout.count() > 0, "invalid global_timeout");
  std::atomic<RequestState> state = RequestState::Running;

  rt::StopSource stop_source;
  rt::StopCallback stop_propagate(stop_token, [&]() {
    RequestState expected = RequestState::Running;
    if (state.compare_exchange_strong(expected, RequestState::Cancelled)) {
      stop_source.Stop();
    }
  });

  boost::fibers::fiber global_timeout_fiber([&]() {
    rt::SleepFor(config.global_timeout, stop_source.GetToken());
    RequestState expected = RequestState::Running;
    if (state.compare_exchange_strong(expected, RequestState::GlobalTimeout)) {
      stop_source.Stop();
    }
  });

  DEFER {
    RequestState expected = RequestState::Running;
    if (state.compare_exchange_strong(expected, RequestState::Finished)) {
      stop_source.Stop();
    }
    global_timeout_fiber.join();
  };

  Request request;
  *request.mutable_command() = command;
  request.set_client_id(client_id_);
  request.set_request_id(rt::GetRandomInt());

  std::optional<Result<google::protobuf::Any, RaftClientError>> result;

  rt::Backoff backoff(config.backoff_params, rt::GetGenerator());

  size_t attempt_id = 0;
  do {
    LOG("RAFT_CLIENT: start attempt {} to {}", attempt_id, raft_nodes_[current_leader_].address);

    result = StartAttempt(request, config.rpc_timeout, stop_source.GetToken());

    if (result->HasError()) {
      current_leader_ = (current_leader_ + 1) % clients_.size();

      LOG("RAFT_CLIENT: attempt {} finished with error: {}", attempt_id,
          result->GetError().Message());

      auto sleep_time = backoff.Next();
      LOG("RAFT_CLIENT: sleep for {}", rt::ToString(sleep_time));
      rt::SleepFor(sleep_time, stop_source.GetToken());
    } else {
      LOG("RAFT_CLIENT: attempt {} finished with success", attempt_id);
    }
    ++attempt_id;
  } while (attempt_id <= config.retry_count && !stop_source.StopRequested() && result->HasError());

  switch (state.load()) {
    case RequestState::Cancelled:
      result = Err(rt::rpc::RpcError(rt::rpc::RpcErrorType::Cancelled));
      break;
    case RequestState::GlobalTimeout:
      result = Err(RaftClientError(RaftClientErrorType::GlobalTimeout));
      break;
    default:
      break;
  }

  if (result->HasError()) {
    LOG("RAFT_CLIENT: finished all attempts, last error: ", result->GetError().Message());
  } else {
    LOG("RAFT_CLIENT: finished all attempts, success");
  }

  return std::move(*result);
}

Result<google::protobuf::Any, RaftClientError> RaftClient::StartAttempt(const Request& request,
                                                                        rt::Duration rpc_timeout,
                                                                        rt::StopToken stop_token) {
  auto& client = *clients_[current_leader_];

  rt::StopSource stop_source;
  rt::StopCallback stop_propagate(stop_token, [&]() {
    stop_source.Stop();
  });

  std::atomic_bool rpc_timed_out = false;

  boost::fibers::fiber rpc_timeout_fiber([&]() {
    rt::SleepFor(rpc_timeout, stop_source.GetToken());
    rpc_timed_out = true;
    stop_source.Stop();
  });

  DEFER {
    stop_source.Stop();
    rpc_timeout_fiber.join();
  };

  return client.Execute(request, stop_source.GetToken())
      .TransformError([&](rt::rpc::RpcError&& error) {
        if (rpc_timed_out) {
          return RaftClientError(RaftClientErrorType::RpcTimeout);
        }
        return RaftClientError(std::move(error));
      })
      .AndThen([&](Response&& response) -> Result<google::protobuf::Any, RaftClientError> {
        if (response.status() == Response::Status::Response_Status_NotALeader) {
          return Err(RaftClientError(RaftClientErrorType::NotLeader));
        }
        return Ok(std::move(*response.mutable_result()));
      });
}

}  // namespace ceq::raft
