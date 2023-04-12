#include "node.h"

#include <chrono>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

#include <raft/raft.client.h>
#include <raft/raft.service.h>

#include <runtime/api.h>
#include <runtime/util/event.h>

#include <util/defer.h>

#include <boost/fiber/all.hpp>

#include "state_machine.h"

namespace ceq::raft {

struct RaftNode final : public rt::rpc::RaftInternalsStub, public rt::rpc::RaftApiStub {
  enum class State {
    Leader,
    Follower,
    Candidate,
  };

  struct PendingRequest {
    rt::Event replication_finished{};
    google::protobuf::Any response{};
    bool success = false;
  };

  explicit RaftNode(IStateMachine* state_machine, RaftConfig config)
      : config{config}, rsm{state_machine} {
    for (auto& endpoint : config.cluster) {
      clients.emplace_back(endpoint);
    }
  }

  //////////////////////////////////////////////////////////
  // API
  //////////////////////////////////////////////////////////

  Result<Response, rt::rpc::Error> Execute(const Request& request) noexcept override {
    LOG("EXECUTE: start");

    Response response;
    if (state != State::Leader) {
      LOG("EXECUTE: fail: not a leader");
      response.set_status(Response_Status_NotALeader);
      return Ok(std::move(response));
    }

    {
      LogEntry entry;
      entry.set_term(current_term);
      *entry.mutable_request() = request;
      log.emplace_back(std::move(entry));
    }

    size_t new_log_index = log.size();

    LOG("EXECUTE: append command to log, index = {}", new_log_index);

    PendingRequest pending_request;

    rt::StopCallback finish_guard(stop_leader.GetToken(), [&]() {
      pending_request.replication_finished.Signal();
    });

    pending_requests[new_log_index] = &pending_request;

    reset_heartbeat_timeout.Stop();

    LOG("EXECUTE: await replicating command to replicas");
    pending_request.replication_finished.Await();

    pending_requests.erase(new_log_index);

    if (!pending_request.success) {
      response.set_status(Response_Status_NotALeader);
      LOG("EXECUTE: fail: current replica is not a leader");
      return Ok(std::move(response));
    }

    LOG("EXECUTE: success");
    response.set_status(Response_Status_Commited);
    *response.mutable_response() = std::move(pending_request.response);

    return Ok(std::move(response));
  }

  //////////////////////////////////////////////////////////
  // Internal RPC's
  //////////////////////////////////////////////////////////

  Result<AppendEntriesResult, rt::rpc::Error> AppendEntries(
      const AppendEntriesRequest& request) noexcept override {
    LOG("APPEND_ENTRIES: start, current_term = {}, request_term = {}", current_term,
        request.term());

    AppendEntriesResult result;
    result.set_term(current_term);

    if (request.term() < current_term) {
      LOG("APPEND_ENTRIES: dismiss, request_term < current_term");
      result.set_success(false);
      return Ok(std::move(result));
    }

    reset_election_timeout.Stop();

    if (request.term() >= current_term) {
      LOG("APPEND_ENTRIES: request_term >= current_term; change to FOLLOWER");
      state = State::Follower;
      stop_election.Stop();
      stop_leader.Stop();
      current_term = request.term();
    }

    if (request.prev_log_index() == 0) {
      VERIFY(request.prev_log_term() == 0, "invalid prev_log_term");
    }

    if (request.prev_log_index() > log.size()) {
      LOG("APPEND_ENTRIES: dismiss: prev_log_index = {} > log_size = {}", request.prev_log_index(),
          log.size());
      result.set_success(false);
      return Ok(std::move(result));
    }

    if (request.prev_log_index() != 0 &&
        log[request.prev_log_index() - 1].term() != request.prev_log_term()) {
      LOG("APPEND_ENTRIES: dismiss: prev_log_term = {} != request_prev_log_term = {}",
          log[request.prev_log_index() - 1].term(), request.prev_log_term());
      result.set_success(false);
      return Ok(std::move(result));
    }

    LOG("APPEND_ENTRIES: accept, writting entries to local log");
    result.set_success(true);
    log.resize(request.prev_log_index());
    for (size_t index = 0; index < request.entries().size(); ++index) {
      log.emplace_back(request.entries()[index]);
    }

    uint64_t new_commit_index = commit_index;
    if (request.leader_commit() > commit_index) {
      new_commit_index = std::min<uint64_t>(request.leader_commit(), log.size());
    }

    LOG("APPEND_ENTRIES: updating commit index, prev = {}, new = {}", commit_index,
        new_commit_index);

    for (size_t index = commit_index + 1; index <= new_commit_index; ++index) {
      rsm.Execute(log[index - 1].request());
    }

    commit_index = new_commit_index;

    return Ok(std::move(result));
  }

  Result<RequestVoteResult, rt::rpc::Error> RequestVote(
      const RequestVoteRequest& request) noexcept override {
    LOG("REQUEST_VOTE: start, current_term = {}, candidate_id = {}, request_term = {}",
        current_term, request.candidate_id(), request.term());
    RequestVoteResult result;
    result.set_term(current_term);
    if (request.term() < current_term) {
      LOG("REQUEST_VOTE: dismiss: request_term < current_term");
      result.set_vote_granted(false);
      return Ok(std::move(result));
    }

    if (request.term() > current_term) {
      LOG("REQUEST_VOTE: request_term > current_term; change to FOLLOWER");
      current_term = request.term();
      state = State::Follower;
      voted_for = std::nullopt;
      reset_election_timeout.Stop();
      stop_election.Stop();
      stop_leader.Stop();
    }

    if (voted_for == request.candidate_id()) {
      LOG("REQUEST_VOTE: accept: voted_for == candidate_id");
      result.set_vote_granted(true);
      return Ok(std::move(result));
    }

    if (voted_for) {
      LOG("REQUEST_VOTE: dismiss: already voted_for = {}", *voted_for);
      result.set_vote_granted(false);
      return Ok(std::move(result));
    }

    const uint64_t last_log_term = GetLastLogTerm();
    if (last_log_term > request.last_log_term()) {
      LOG("REQUEST_VOTE: dismiss: current last_log_term = {} > candidate last_log_term = {}",
          last_log_term, request.last_log_term());
      result.set_vote_granted(false);
    } else if (last_log_term == request.last_log_term()) {
      LOG("REQUEST_VOTE: current last_log_term = {} == candidate last_log_term = {}", last_log_term,
          request.last_log_term());
      if (log.size() <= request.last_log_index()) {
        LOG("REQUEST_VOTE: accept: current log_size = {} <= candidate log_size {}", log.size(),
            request.last_log_index());
        result.set_vote_granted(true);
      } else {
        LOG("REQUEST_VOTE: dismiss: current log_size = {} > candidate log_size {}", log.size(),
            request.last_log_index());
        result.set_vote_granted(false);
      }
    } else {
      LOG("REQUEST_VOTE: accept: current last_log_term = {} < candidate last_log_term = {}",
          last_log_term, request.last_log_term());
      result.set_vote_granted(true);
    }
    if (result.vote_granted()) {
      voted_for = request.candidate_id();
    }
    return Ok(std::move(result));
  }

  //////////////////////////////////////////////////////////
  // Node state
  //////////////////////////////////////////////////////////

  void StartNode() noexcept {
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
    LOG("LEADER: start with term {}", current_term);
    DEFER {
      LOG("LEADER: finished");
    };

    next_index = std::vector<uint64_t>(config.cluster.size(), log.size() + 1);
    match_index = std::vector<uint64_t>(config.cluster.size(), 0);

    std::vector<boost::fibers::fiber> sessions;

    stop_leader = rt::StopSource();

    for (size_t node_id = 0; node_id < config.cluster.size(); ++node_id) {
      if (node_id == config.node_id) {
        continue;
      }
      sessions.emplace_back([&, node_id]() {
        StartLeaderNodeSession(node_id);
      });
    }

    for (auto& session : sessions) {
      session.join();
    }
  }

  void StartLeaderNodeSession(size_t node_id) noexcept {
    while (state == State::Leader) {
      VERIFY(next_index[node_id] >= 1, "invalid next_index");
      AppendEntriesRequest request;

      request.set_term(current_term);
      request.set_leader_id(config.node_id);
      request.set_prev_log_index(next_index[node_id] - 1);
      request.set_prev_log_term(next_index[node_id] == 1 ? 0 : log[next_index[node_id] - 2].term());

      for (size_t index = next_index[node_id] - 1; index < log.size(); ++index) {
        *request.add_entries() = log[index];
      }

      rt::StopSource request_stop;
      rt::StopCallback stop_propagate(stop_leader.GetToken(), [&]() {
        request_stop.Stop();
      });

      boost::fibers::fiber request_timeout([&]() {
        rt::SleepFor(config.rpc_timeout, request_stop.GetToken());
        request_stop.Stop();
      });

      DEFER {
        request_stop.Stop();
        request_timeout.join();
      };

      LOG("LEADER[{}]: start AppendEntries with next_index = {}, match_index = {}", node_id,
          next_index[node_id], match_index[node_id]);
      auto result = clients[node_id].AppendEntries(request, request_stop.GetToken());
      LOG("LEADER[{}]: finished AppendEntries", node_id);

      if (state != State::Leader) {
        LOG("LEADER[{}]: not a leader", node_id);
        continue;
      }

      if (result.HasError()) {
        LOG("LEADER[{}]: AppendEntries end with error: {}", node_id, result.GetError().Message());
        continue;
      }

      AppendEntriesResult& response = result.GetValue();
      if (response.term() > current_term) {
        LOG("LEADER[{}]: received greater term = {}, change state to FOLLOWER", node_id,
            response.term());
        current_term = response.term();
        state = State::Follower;
        stop_leader.Stop();
        continue;
      }

      if (!response.success()) {
        LOG("LEADER[{}]: log is inconsistent, decrement next_index", node_id);
        VERIFY(next_index[node_id] >= 2, "invalid state");
        --next_index[node_id];
      } else {
        match_index[node_id] = request.prev_log_index() + request.entries_size();
        next_index[node_id] += request.entries_size();
        LOG("LEADER[{}]: success, update match_index = {} and next_index = {}", node_id,
            match_index[node_id], next_index[node_id]);

        UpdateCommitIndex();
      }

      if (next_index[node_id] == log.size() + 1) {
        LOG("LEADER[{}]: all log replicated, wait on heart_beat_timeout", node_id);
        reset_heartbeat_timeout = rt::StopSource();
        rt::SleepFor(config.heart_beat_period, reset_heartbeat_timeout.GetToken());
      }
    }
  }

  void UpdateCommitIndex() noexcept {
    VERIFY(state == State::Leader, "UpdateCommitIndex must be called only by leaders");

    uint64_t new_commit_index = commit_index;

    for (uint64_t index = log.size(); index > commit_index && log[index - 1].term() == current_term;
         --index) {
      size_t match_count = 1;

      for (size_t node_id = 0; node_id < config.cluster.size(); ++node_id) {
        if (node_id == config.node_id) {
          continue;
        }
        if (match_index[node_id] >= index) {
          ++match_count;
        }
      }
      if (match_count >= MajorityCount()) {
        new_commit_index = index;
        break;
      }
    }

    LOG("LEADER: updating commit index, prev = {}, new = {}", commit_index, new_commit_index);

    for (uint64_t index = commit_index + 1; index <= new_commit_index; ++index) {
      auto response = rsm.Execute(log[index - 1].request());
      auto it = pending_requests.find(index);
      if (it != pending_requests.end()) {
        it->second->response = std::move(response);
        it->second->success = true;
        it->second->replication_finished.Signal();
      }
    }

    commit_index = new_commit_index;
  }

  void StartFollower() noexcept {
    LOG("FOLLOWER: start with term {}", current_term);
    DEFER {
      LOG("FOLLOWER: finished");
    };

    while (state == State::Follower) {
      reset_election_timeout = rt::StopSource();
      if (!rt::SleepFor(GetElectionTimeout(), reset_election_timeout.GetToken())) {
        LOG("FOLLOWER: election timeout expired, change state to CANDIDATE");
        state = State::Candidate;
      } else {
        LOG("FOLLOWER: election timeout was cancelled, continue as FOLLOWER");
      }
    }
  }

  void StartCandidate() noexcept {
    DEFER {
      LOG("CANDIDATE: finished");
    };

    while (state == State::Candidate) {
      ++current_term;
      voted_for = config.node_id;

      LOG("CANDIDATE: start new elections with term {}", current_term);

      size_t votes_count = 1;

      RequestVoteRequest request_vote;
      request_vote.set_term(current_term);
      request_vote.set_candidate_id(config.node_id);
      request_vote.set_last_log_term(GetLastLogTerm());
      request_vote.set_last_log_index(log.size());

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
          LOG("CANDIDATE: start RequestVote to node {}", node_id);
          Result<RequestVoteResult, rt::rpc::Error> result =
              clients[node_id].RequestVote(request_vote);

          if (result.HasError()) {
            LOG("CANDIDATE: finished RequestVote to node {} with error: {}", node_id,
                result.GetError().Message());
            return;
          }

          if (state != State::Candidate) {
            LOG("CANDIDATE: finished RequestVote to node {}, not a CANDIDATE", node_id);
            return;
          }

          if (result.GetValue().vote_granted()) {
            LOG("CANDIDATE: node {} granted vote", node_id);
            ++votes_count;
          } else {
            LOG("CANDIDATE: node {} denied vote", node_id);
          }

          if (votes_count >= MajorityCount()) {
            LOG("CANDIDATE: got majority of votes, change state to LEADER");
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
  }

  rt::Duration GetElectionTimeout() noexcept {
    std::uniform_int_distribution<rt::Duration::rep> dist{
        config.election_timeout_interval.first.count(),
        config.election_timeout_interval.second.count()};
    return rt::Duration(dist(rt::GetGenerator()));
  }

  size_t MajorityCount() const noexcept {
    return config.cluster.size() / 2 + 1;
  }

  uint64_t GetLastLogTerm() const noexcept {
    return log.empty() ? 0 : log.back().term();
  }

  rt::StopSource stop_election;
  rt::StopSource stop_leader;

  State state = State::Follower;

  RaftConfig config;
  std::vector<rt::rpc::RaftInternalsClient> clients;

  rt::StopSource reset_election_timeout;
  rt::StopSource reset_heartbeat_timeout;

  std::unordered_map<uint64_t, PendingRequest*> pending_requests;

  // Persistent state
  // TODO: write to disk
  uint64_t current_term = 0;
  std::optional<uint64_t> voted_for;
  std::vector<LogEntry> log;

  // Volatile state
  uint64_t commit_index = 0;
  uint64_t last_applied = 0;

  // Leaders volatile state
  std::vector<uint64_t> match_index;
  std::vector<uint64_t> next_index;

  // State machine
  impl::StateMachine rsm;
};

void RunMain(IStateMachine* state_machine, RaftConfig config) noexcept {
  RaftNode node(state_machine, config);

  // Raft node does not support concurrent execution
  rt::rpc::ServerRunConfig server_config;
  server_config.worker_threads_count = 1;
  server_config.threads_per_queue = 1;
  server_config.queue_count = 1;

  rt::rpc::Server server;
  server.Register(static_cast<rt::rpc::RaftInternalsStub*>(&node));
  server.Register(static_cast<rt::rpc::RaftApiStub*>(&node));
  server.Run(config.cluster[config.node_id].port, server_config);

  node.StartNode();
  server.ShutDown();
}

}  // namespace ceq::raft
