#include "node.h"

#include <chrono>
#include <optional>

#include <raft/raft.client.h>
#include <raft/raft.service.h>
#include <runtime/api.h>

using namespace std::chrono_literals;

namespace ceq::raft {

enum class State {
  Leader,
  Follower,
  Candidate,
};

struct RaftNode final : public rt::RaftInternalsStub, public rt::RaftApiStub {
  explicit RaftNode(StartConfig config) : config{std::move(config)} {
    for (auto& endpoint : config.cluster) {
      cluster.emplace_back(endpoint);
    }
  }

  Result<AppendEntriesResult, rt::RpcError> AppendEntries(
      const AppendEntriesRequest& request) noexcept override;

  Result<RequestVoteResult, rt::RpcError> RequestVote(
      const RequestVoteRequest& request) noexcept override;

  Result<Response, rt::RpcError> Execute(const Command& request) noexcept override;

  void StartNode() noexcept {
  }

  State state = State::Candidate;

  StartConfig config;
  std::vector<rt::RaftInternalsClient> cluster;

  // Persistent state
  uint64_t current_term = 0;
  std::optional<uint64_t> voted_for;
  std::vector<LogEntry> log;  // First index is 1

  // Volatile state
  uint64_t commit_index = 0;
  uint64_t last_applied = 0;

  // Volatile state on leaders
  std::vector<uint64_t> next_index;
  std::vector<uint64_t> match_index;
};

void RunMain(StartConfig config) noexcept {
  RaftNode node(config);

  rt::RpcServer server;
  server.Register(static_cast<rt::RaftInternalsStub*>(&node));
  server.Register(static_cast<rt::RaftApiStub*>(&node));
  server.Run(config.port);

  rt::SleepFor(10h);

  server.ShutDown();
}

}  // namespace ceq::raft
