#include "node.h"

#include <chrono>
#include <optional>
#include <vector>

#include <raft/raft.client.h>
#include <raft/raft.service.h>

#include <runtime/api.h>
#include <runtime/cancellation/stop_source.h>
#include <runtime/event.h>

#include <util/defer.h>

#include <boost/fiber/all.hpp>

using namespace std::chrono_literals;

namespace ceq::raft {

enum class State {
  Leader,
  Follower,
  Candidate,
};

struct StateMachine {};

struct RaftNode final : public rt::RaftInternalsStub, public rt::RaftApiStub {
  explicit RaftNode(RaftConfig config) : config{config} {
    for (auto& endpoint : config.cluster) {
      clients.emplace_back(endpoint);
    }
  }

  //////////////////////////////////////////////////////////
  // API
  //////////////////////////////////////////////////////////

  Result<Response, rt::RpcError> Execute(const Command& command) noexcept override {
    Response response;
    response.set_status(Response_Status_NotALeader);
    if (state != State::Leader) {
      return Ok(std::move(response));
    }

    LogEntry entry;
    entry.set_term(current_term);
    entry.set_data(command.data());
    log.emplace_back(entry);

    size_t new_log_index = log.size();

    std::unique_lock guard(commit_index_lock);
    commit_index_update.wait(guard, [&]() {
      return state != State::Leader || commit_index >= new_log_index;
    });

    if (state != State::Leader) {
      return Ok(std::move(response));
    }

    response.set_status(Response_Status_Commited);

    return Ok(std::move(response));
  }

  //////////////////////////////////////////////////////////
  // Internal RPC's
  //////////////////////////////////////////////////////////

  Result<AppendEntriesResult, rt::RpcError> AppendEntries(
      const AppendEntriesRequest& request) noexcept override {
    LOG("start AppendEntries handler with received term {}, current term {}", request.term(),
        current_term);

    AppendEntriesResult result;
    result.set_term(current_term);

    if (request.term() < current_term) {
      result.set_success(false);
      return Ok(std::move(result));
    }

    reset_election_timeout.Stop();

    if (request.term() >= current_term) {
      state = State::Follower;
      stop_election.Stop();
      stop_leader.Stop();
      commit_index_update.notify_all();
      current_term = request.term();
    }

    if (request.prev_log_index() == 0) {
      VERIFY(request.prev_log_term() == 0, "invalid prev_log_term");
      result.set_success(true);
      return Ok(std::move(result));
    }

    if (request.prev_log_index() > log.size()) {
      result.set_success(false);
      return Ok(std::move(result));
    }

    if (log[request.prev_log_index() - 1].term() != request.prev_log_term()) {
      result.set_success(false);
      return Ok(std::move(result));
    }

    result.set_success(true);
    log.resize(request.prev_log_index());
    for (size_t index = 0; index < request.entries().size(); ++index) {
      log.emplace_back(request.entries()[index]);
    }

    if (request.leader_commit() > commit_index) {
      commit_index = std::min<uint64_t>(request.leader_commit(), log.size());
    }

    // TODO: Apply to state machine

    return Ok(std::move(result));
  }

  Result<RequestVoteResult, rt::RpcError> RequestVote(
      const RequestVoteRequest& request) noexcept override {
    LOG("RequestVote: start handler with term {}", request.term());
    RequestVoteResult result;
    result.set_term(current_term);
    if (request.term() < current_term) {
      result.set_vote_granted(false);
      return Ok(std::move(result));
    }

    if (request.term() > current_term) {
      current_term = request.term();
      state = State::Follower;
      voted_for = std::nullopt;
      stop_election.Stop();
      stop_leader.Stop();
      commit_index_update.notify_all();
    }

    if (voted_for == request.candidate_id()) {
      result.set_vote_granted(true);
      return Ok(std::move(result));
    }

    if (voted_for) {
      result.set_vote_granted(false);
      return Ok(std::move(result));
    }

    const uint64_t last_log_term = log.empty() ? 0 : log.back().term();
    if (last_log_term > request.last_log_term()) {
      result.set_vote_granted(false);
    } else if (last_log_term == request.last_log_term()) {
      result.set_vote_granted(log.size() <= request.last_log_index());
    } else {
      result.set_vote_granted(true);
    }
    return Ok(std::move(result));
  }

  //////////////////////////////////////////////////////////
  // Node state
  //////////////////////////////////////////////////////////

  void StartNode() noexcept {
    LOG("starting node");

    while (true) {
      switch (state) {
        case State::Leader:
          StartLeader();
          break;
        case State::Follower:
          StartFollower();
          break;
        case State::Candidate:
          StartCandidate();
          break;
      }
    }
  }

  void StartLeader() noexcept {
    LOG("StartLeader");

    std::vector<uint64_t> match_index(config.cluster.size(), 0);

    std::vector<boost::fibers::fiber> sessions;

    stop_leader = rt::StopSource();

    for (size_t node_id = 0; node_id < config.cluster.size(); ++node_id) {
      if (node_id == config.node_id) {
        continue;
      }
      sessions.emplace_back([&, node_id]() {
        StartLeaderNodeSession(node_id, match_index);
      });
    }

    for (auto& session : sessions) {
      session.join();
    }

    LOG("finished StartLeader");
  }

  void StartLeaderNodeSession(size_t node_id, std::vector<uint64_t> match_index) noexcept {
    size_t next_index = log.size() + 1;

    while (state == State::Leader) {
      AppendEntriesRequest request;

      request.set_term(current_term);
      request.set_leader_id(config.node_id);
      request.set_prev_log_index(next_index - 1);
      request.set_prev_log_term(next_index == 1 ? 0 : log[next_index - 2].term());

      for (size_t index = next_index - 1; index < log.size(); ++index) {
        auto entry = request.add_entries();
        entry->set_term(log[index].term());
        entry->set_data(log[index].data());
      }

      rt::StopSource request_stop;
      rt::StopCallback stop_guard(stop_leader.GetToken(), [&]() {
        request_stop.Stop();
      });

      boost::fibers::fiber request_timeout([&]() {
        rt::SleepFor(config.rpc_timeout, request_stop.GetToken());
        request_stop.Stop();
      });

      DEFER {
        request_timeout.join();
      };

      auto result = clients[node_id].AppendEntries(request, request_stop.GetToken());

      if (result.HasError()) {
        continue;
      }

      AppendEntriesResult& response = result.GetValue();
      if (response.term() > current_term) {
        current_term = response.term();
        state = State::Follower;
        stop_leader.Stop();
        commit_index_update.notify_all();
        continue;
      }

      if (!response.success()) {
        VERIFY(next_index >= 2, "invalid state");
        --next_index;
      } else {
        match_index[node_id] = next_index;
        ++next_index;

        // TODO: update commit index
      }
    }
  }

  void StartFollower() noexcept {
    LOG("StartFollower");

    while (state == State::Follower) {
      reset_election_timeout = rt::StopSource();
      if (!rt::SleepFor(GetElectionTimeout(), reset_election_timeout.GetToken())) {
        LOG("election timeout expired, change state to candidate");
        state = State::Candidate;
      } else {
        LOG("election timeout was cancelled, continue as follower");
      }
    }

    LOG("finished StartFollower");
  }

  void StartCandidate() noexcept {
    LOG("StartCandidate");

    while (state == State::Candidate) {
      ++current_term;
      voted_for = config.node_id;

      LOG("start new election with term {}", current_term);

      size_t votes_count = 1;
      const size_t quorum = config.cluster.size() / 2 + 1;

      RequestVoteRequest request_vote;
      request_vote.set_term(current_term);
      request_vote.set_candidate_id(config.node_id);

      stop_election = rt::StopSource{};

      boost::fibers::fiber election_timer([&]() {
        rt::SleepFor(GetElectionTimeout(), stop_election.GetToken());
        stop_election.Stop();
      });

      std::vector<boost::fibers::fiber> requests;

      for (size_t node_id = 0; node_id < config.cluster.size(); ++node_id) {
        if (node_id == config.node_id) {
          continue;
        }
        requests.emplace_back([&, node_id]() {
          LOG("start RequestVote to node {}", node_id);
          Result<RequestVoteResult, rt::RpcError> result =
              clients[node_id].RequestVote(request_vote);
          if (result.HasError()) {
            LOG("finished RequestVote to node {} with error: {}", node_id,
                result.GetError().Message());
          } else {
            LOG("finished RequestVote to node {} with success: {}", node_id, result.GetValue());
          }

          if (state != State::Candidate || result.HasError()) {
            return;
          }

          if (result.GetValue().vote_granted()) {
            ++votes_count;
          }

          if (votes_count >= quorum) {
            state = State::Leader;
            stop_election.Stop();
          }
        });
      }

      for (auto& request : requests) {
        request.join();
      }

      election_timer.join();
    }
    LOG("finished StartCandidate");
  }

  rt::Duration GetElectionTimeout() noexcept {
    std::uniform_int_distribution<rt::Duration::rep> dist{
        config.election_timeout_interval.first.count(),
        config.election_timeout_interval.second.count()};
    return rt::Duration(dist(rt::GetGenerator()));
  }

  rt::StopSource stop_election;
  rt::StopSource stop_leader;

  rt::Event append_entries_quorum_received;
  boost::fibers::fiber send_append_entries_tasks;
  rt::StopSource cancel_append_entries_tasks;

  State state = State::Follower;

  RaftConfig config;
  std::vector<rt::RaftInternalsClient> clients;

  rt::StopSource reset_election_timeout;

  boost::fibers::mutex commit_index_lock;
  boost::fibers::condition_variable commit_index_update;

  // Persistent state
  uint64_t current_term = 0;
  std::optional<uint64_t> voted_for;
  std::vector<LogEntry> log;  // First index is 1

  // Volatile state
  uint64_t commit_index = 0;
  uint64_t last_applied = 0;
};

void RunMain(RaftConfig config) noexcept {
  RaftNode node(config);

  rt::RpcServer server;
  server.Register(static_cast<rt::RaftInternalsStub*>(&node));
  server.Register(static_cast<rt::RaftApiStub*>(&node));
  server.Run(config.cluster[config.node_id].port);

  node.StartNode();
  server.ShutDown();
}

}  // namespace ceq::raft
