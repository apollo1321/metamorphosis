#include "node.h"

#include "state_machine_wrapper.h"

#include <raft/raft.client.h>
#include <raft/raft.service.h>

#include <runtime/api.h>
#include <runtime/util/event/event.h>
#include <runtime/util/serde/protobuf_serde.h>
#include <runtime/util/serde/string_serde.h>
#include <runtime/util/serde/u64_serde.h>

#include <util/defer.h>

#include <boost/fiber/all.hpp>

#include <google/protobuf/util/message_differencer.h>

#include <chrono>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace mtf::raft {

using RaftStateDbPtr = rt::kv::KVStoragePtr<rt::serde::StringSerde, rt::serde::U64Serde>;
using RaftLogDbPtr = rt::kv::KVStoragePtr<rt::serde::U64Serde, rt::serde::ProtobufSerde<LogEntry>>;

struct RaftNode final : public rt::rpc::RaftInternalsStub, public rt::rpc::RaftApiStub {
  enum class State {
    Leader,
    Follower,
    Candidate,
  };

  struct PendingRequest {
    rt::Event replication_finished{};
    google::protobuf::Any result{};
  };

  explicit RaftNode(IStateMachine* state_machine, RaftConfig config, RaftStateDbPtr state_db,
                    RaftLogDbPtr log_db)
      : config{config},
        rsm{state_machine},
        raft_state{std::move(state_db)},
        raft_log{std::move(log_db)} {
    {
      // Setup current_term
      auto result = raft_state->Get("current_term");
      if (result.HasError()) {
        raft_state->Put("current_term", 0).ExpectOk();
      }
    }

    for (auto& endpoint : config.raft_nodes) {
      clients.emplace_back(std::make_unique<rt::rpc::RaftInternalsClient>(endpoint));
    }
  }

  //////////////////////////////////////////////////////////
  // API
  //////////////////////////////////////////////////////////

  Result<Response, rt::rpc::RpcError> Execute(const Request& request) noexcept override {
    LOG_DBG("EXECUTE: start");

    Response response;
    if (state != State::Leader) {
      LOG_DBG("EXECUTE: fail: not a leader");
      response.set_status(Response_Status_NotALeader);
      return Ok(std::move(response));
    }

    uint64_t new_log_index = next_index[config.node_id]++;
    uint64_t current_term = GetCurrentTerm();

    {
      LogEntry entry;
      entry.set_term(current_term);
      *entry.mutable_request() = request;
      pending_log.emplace_back(entry);
    }

    LOG_DBG("EXECUTE: append command to log, index = {}", new_log_index);

    PendingRequest pending_request;

    rt::StopCallback finish_guard(stop_leader.GetToken(), [&pending_request]() {
      pending_request.replication_finished.Signal();
    });

    pending_requests[new_log_index] = &pending_request;

    reset_heartbeat_timeout.Stop();
    reset_heartbeat_timeout = rt::StopSource();

    LOG_DBG("EXECUTE: await replicating command to replicas");
    pending_request.replication_finished.Await();

    pending_requests.erase(new_log_index);

    if (current_term != GetCurrentTerm()) {
      response.set_status(Response_Status_NotALeader);
      LOG_DBG("EXECUTE: fail: current replica is not a leader");
      return Ok(std::move(response));
    }

    VERIFY(state == State::Leader, "Invalid session state");

    LOG_DBG("EXECUTE: success");
    response.set_status(Response_Status_Commited);
    *response.mutable_result() = std::move(pending_request.result);

    return Ok(std::move(response));
  }

  //////////////////////////////////////////////////////////
  // Internal RPC's
  //////////////////////////////////////////////////////////

  Result<AppendEntriesResult, rt::rpc::RpcError> AppendEntries(
      const AppendEntriesRequest& request) noexcept override {
    AppendEntriesResult result;
    result.set_term(GetCurrentTerm());
    result.set_success(false);

    LOG("APPEND_ENTRIES: start, current_term = {}, request_term = {}, log_size = {}",
        GetCurrentTerm(), request.term(), request.entries_size());

    if (request.term() < GetCurrentTerm()) {
      LOG_DBG("APPEND_ENTRIES: dismiss, request_term < current_term");
      return Ok(std::move(result));
    }

    reset_election_timeout.Stop();

    if (request.term() >= GetCurrentTerm() && state != State::Follower) {
      LOG_DBG("APPEND_ENTRIES: request_term >= current_term; become to FOLLOWER");
      BecomeFollower();
    }

    if (request.term() > GetCurrentTerm()) {
      UpdateTerm(request.term());
    }

    VERIFY(request.prev_log_index() != 0 || request.prev_log_term() == 0, "invalid prev_log_term");

    if (request.prev_log_index() > GetSavedLogSize()) {
      LOG_DBG("APPEND_ENTRIES: dismiss: prev_log_index = {} > log_size = {}",
              request.prev_log_index(), GetSavedLogSize());
      return Ok(std::move(result));
    }

    if (request.prev_log_index() != 0) {
      const uint64_t log_term = raft_log->Get(request.prev_log_index()).GetValue().term();
      if (log_term != request.prev_log_term()) {
        LOG_DBG("APPEND_ENTRIES: dismiss: prev_log_term = {} != request_prev_log_term = {}",
                log_term, request.prev_log_term());
        return Ok(std::move(result));
      }
    }

    LOG_DBG("APPEND_ENTRIES: accept, writting entries to local log");
    result.set_success(true);

    // Check that if leader tries to overwrite the commited log, it must be the same.
    // The situation when the leader overwrites commited log may occur in case of rpc retries.
    for (uint64_t index = request.prev_log_index() + 1;
         index <= commit_index && index - request.prev_log_index() - 1 < request.entries_size();
         ++index) {
      using google::protobuf::util::MessageDifferencer;

      LogEntry value_from_log = raft_log->Get(index).GetValue();
      LogEntry value_from_request = request.entries()[index - request.prev_log_index() - 1];

      VERIFY(MessageDifferencer::Equals(value_from_request, value_from_log),
             "invalid state: committed log is inconsistent");
    }

    auto write_batch = raft_log->MakeWriteBatch();
    write_batch
        .DeleteRange(std::max(request.prev_log_index(), commit_index) + 1,
                     std::numeric_limits<uint64_t>::max())
        .ExpectOk();
    for (uint64_t index = std::max(request.prev_log_index(), commit_index) + 1;
         index - request.prev_log_index() - 1 < request.entries_size(); ++index) {
      write_batch.Put(index, request.entries()[index - request.prev_log_index() - 1]).ExpectOk();
    }
    raft_log->Write(std::move(write_batch)).ExpectOk();

    const uint64_t new_commit_index =
        std::max(commit_index, std::min<uint64_t>(request.leader_commit(), GetSavedLogSize()));

    if (commit_index != new_commit_index) {
      LOG_DBG("APPEND_ENTRIES: updating commit index, prev = {}, new = {}", commit_index,
              new_commit_index);
    }

    for (size_t index = commit_index + 1; index <= new_commit_index; ++index) {
      rsm.Apply(raft_log->Get(index).GetValue().request());
    }

    commit_index = new_commit_index;

    return Ok(std::move(result));
  }

  Result<RequestVoteResult, rt::rpc::RpcError> RequestVote(
      const RequestVoteRequest& request) noexcept override {
    LOG("REQUEST_VOTE: start, current_term = {}, candidate_id = {}, request_term = {}",
        GetCurrentTerm(), request.candidate_id(), request.term());
    RequestVoteResult result;
    result.set_term(GetCurrentTerm());
    if (request.term() < GetCurrentTerm()) {
      LOG_DBG("REQUEST_VOTE: dismiss: request_term < current_term");
      result.set_vote_granted(false);
      return Ok(std::move(result));
    }

    if (request.term() > result.term()) {
      LOG_DBG("REQUEST_VOTE: request_term > current_term; change to FOLLOWER");
      UpdateTerm(request.term());
      if (state != State::Follower) {
        BecomeFollower();
      }
    }

    const auto voted_for = GetVotedFor();

    if (voted_for == request.candidate_id()) {
      LOG_DBG("REQUEST_VOTE: accept: voted_for == candidate_id");
      result.set_vote_granted(true);
      return Ok(std::move(result));
    }

    if (voted_for) {
      LOG_DBG("REQUEST_VOTE: dismiss: already voted_for = {}", *voted_for);
      result.set_vote_granted(false);
      return Ok(std::move(result));
    }

    const uint64_t last_log_term = GetLastSavedLogTerm();
    if (last_log_term > request.last_log_term()) {
      LOG_DBG("REQUEST_VOTE: dismiss: (current last_log_term = {}) > (candidate last_log_term = {})",
              last_log_term, request.last_log_term());
      result.set_vote_granted(false);
    } else if (last_log_term == request.last_log_term()) {
      LOG_DBG("REQUEST_VOTE: (current last_log_term = {}) == (candidate last_log_term = {})",
              last_log_term, request.last_log_term());
      if (GetSavedLogSize() <= request.last_log_index()) {
        LOG_DBG("REQUEST_VOTE: accept: (current log_size = {}) <= (candidate log_size = {})",
                GetSavedLogSize(), request.last_log_index());
        result.set_vote_granted(true);
      } else {
        LOG_DBG("REQUEST_VOTE: dismiss: (current log_size = {}) > (candidate log_size {})",
                GetSavedLogSize(), request.last_log_index());
        result.set_vote_granted(false);
      }
    } else {
      LOG_DBG("REQUEST_VOTE: accept: (current last_log_term = {}) < (candidate last_log_term = {})",
              last_log_term, request.last_log_term());
      result.set_vote_granted(true);
    }
    if (result.vote_granted()) {
      raft_state->Put("voted_for", request.candidate_id()).ExpectOk();
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

  //////////////////////////////////////////////////////////
  // Change state
  //////////////////////////////////////////////////////////

  void BecomeLeader() noexcept {
    LOG("Become LEADER in term {}", GetCurrentTerm());
    VERIFY(state == State::Candidate, "Invalid state");
    pending_log.clear();
    current_term_start_index = GetSavedLogSize() + 1;
    next_index = std::vector<uint64_t>(config.raft_nodes.size(), GetSavedLogSize() + 1);
    match_index = std::vector<uint64_t>(config.raft_nodes.size(), 0);
    reset_heartbeat_timeout = rt::StopSource();
    stop_leader = rt::StopSource();
    stop_election.Stop();
    state = State::Leader;
  }

  void BecomeFollower() noexcept {
    LOG("Become FOLLOWER in term {}", GetCurrentTerm());
    VERIFY(state != State::Follower, "Invalid state");
    if (state == State::Leader) {
      stop_leader.Stop();
    } else if (state == State::Candidate) {
      stop_election.Stop();
    }
    state = State::Follower;
  }

  void BecomeCandidate() noexcept {
    LOG("Become CANDIDATE in term {}", GetCurrentTerm());
    VERIFY(state == State::Follower, "Invalid state");
    reset_election_timeout.Stop();
    state = State::Candidate;
  }

  //////////////////////////////////////////////////////////
  // State-specific code
  //////////////////////////////////////////////////////////

  void StartLeader() noexcept {
    std::vector<boost::fibers::fiber> sessions;

    for (size_t node_id = 0; node_id < config.raft_nodes.size(); ++node_id) {
      if (node_id == config.node_id) {
        continue;
      }
      sessions.emplace_back([this, node_id]() {
        StartLeaderNodeSession(node_id);
      });
    }

    sessions.emplace_back([this]() {
      StartLeaderMainSession();
    });

    for (auto& session : sessions) {
      session.join();
    }

    pending_log.clear();
  }

  void StartLeaderMainSession() noexcept {
    match_index[config.node_id] = GetSavedLogSize();
    while (state == State::Leader) {
      LOG("LEADER[{}]: pending log size = {}", config.node_id, pending_log.size());
      auto write_batch = raft_log->MakeWriteBatch();
      for (size_t index = 0; index < pending_log.size(); ++index) {
        write_batch.Put(match_index[config.node_id] + index + 1, pending_log[index]).ExpectOk();
      }
      raft_log->Write(std::move(write_batch)).ExpectOk();
      match_index[config.node_id] += pending_log.size();
      pending_log.clear();

      UpdateCommitIndex();

      rt::StopCallback leader_cancel_propagate(stop_leader.GetToken(), [this]() {
        reset_heartbeat_timeout.Stop();
      });

      rt::Event wake_up;

      rt::StopCallback wake_up_guard(reset_heartbeat_timeout.GetToken(), [&wake_up]() {
        wake_up.Signal();
      });

      wake_up.Await();
    }
  }

  void StartLeaderNodeSession(size_t node_id) noexcept {
    while (state == State::Leader) {
      VERIFY(next_index[node_id] >= 1, "invalid next_index");
      AppendEntriesRequest request;

      request.set_term(GetCurrentTerm());
      request.set_leader_id(config.node_id);
      request.set_prev_log_index(next_index[node_id] - 1);
      request.set_prev_log_term(
          next_index[node_id] == 1 ? 0 : raft_log->Get(next_index[node_id] - 1).GetValue().term());
      request.set_leader_commit(commit_index);

      for (size_t index = next_index[node_id]; index < next_index[config.node_id]; ++index) {
        *request.add_entries() = GetLogEntry(index);
      }

      rt::StopSource request_stop;
      rt::StopCallback request_stop_propagate(stop_leader.GetToken(), [&request_stop]() {
        request_stop.Stop();
      });

      boost::fibers::fiber request_timeout([this, &request_stop]() {
        rt::SleepFor(config.rpc_timeout, request_stop.GetToken());
        request_stop.Stop();
      });

      DEFER {
        request_stop.Stop();
        if (request_timeout.joinable()) {
          request_timeout.join();
        }
      };

      LOG("LEADER[{}]: start AppendEntries with next_index = {}, log_size = {}", node_id,
          next_index[node_id], request.entries_size());
      auto result = clients[node_id]->AppendEntries(request, request_stop.GetToken());

      if (state != State::Leader) {
        LOG_ERR("LEADER[{}]: Not a leader", node_id);
        continue;
      }

      if (result.HasError()) {
        LOG_ERR("LEADER[{}]: AppendEntries RPC has failed with error: {}", node_id,
                result.GetError().Message());
        request_timeout.join();  // Sleep for some time
        continue;
      }

      AppendEntriesResult& response = result.GetValue();
      if (response.term() > GetCurrentTerm()) {
        LOG_DBG("LEADER[{}]: received greater term = {}, change state to FOLLOWER", node_id,
                response.term());
        UpdateTerm(response.term());
        if (state != State::Follower) {
          BecomeFollower();
        }
        continue;
      }

      if (!response.success()) {
        LOG_DBG("LEADER[{}]: log is inconsistent, decrement next_index", node_id);
        VERIFY(next_index[node_id] >= 2, "invalid state");
        --next_index[node_id];
        continue;
      }

      match_index[node_id] = request.prev_log_index() + request.entries_size();
      next_index[node_id] += request.entries_size();
      LOG_DBG("LEADER[{}]: success, update match_index = {} and next_index = {}", node_id,
              match_index[node_id], next_index[node_id]);

      UpdateCommitIndex();

      if (next_index[node_id] == next_index[config.node_id]) {
        LOG_DBG("LEADER[{}]: all log replicated, wait on heart_beat_timeout", node_id);

        rt::StopCallback heart_beat_stop_propagate(stop_leader.GetToken(), [this]() {
          reset_heartbeat_timeout.Stop();
        });

        rt::SleepFor(config.heart_beat_period, reset_heartbeat_timeout.GetToken());
      }
    }
  }

  void UpdateCommitIndex() noexcept {
    uint64_t new_commit_index = commit_index;

    for (uint64_t index = GetSavedLogSize();
         index > commit_index && index >= current_term_start_index; --index) {
      size_t match_count = 0;
      for (size_t node_id = 0; node_id < config.raft_nodes.size(); ++node_id) {
        if (match_index[node_id] >= index) {
          ++match_count;
        }
      }
      if (match_count >= MajorityCount()) {
        new_commit_index = index;
        break;
      }
    }

    if (commit_index != new_commit_index) {
      LOG_DBG("LEADER: updating commit index, prev = {}, new = {}", commit_index, new_commit_index);
    }

    for (uint64_t index = commit_index + 1; index <= new_commit_index; ++index) {
      auto result = rsm.Apply(raft_log->Get(index).GetValue().request());
      auto it = pending_requests.find(index);
      if (it != pending_requests.end()) {
        it->second->result = std::move(result);
        it->second->replication_finished.Signal();
      }
    }

    commit_index = new_commit_index;
  }

  void StartFollower() noexcept {
    reset_election_timeout = rt::StopSource();
    if (!rt::SleepFor(GetElectionTimeout(), reset_election_timeout.GetToken())) {
      LOG_DBG("FOLLOWER: election timeout expired");
      BecomeCandidate();
    } else {
      LOG_DBG("FOLLOWER: election timeout was cancelled");
    }
  }

  void StartCandidate() noexcept {
    stop_election = rt::StopSource{};
    boost::fibers::fiber election_timer([this]() {
      rt::SleepFor(GetElectionTimeout(), stop_election.GetToken());
      stop_election.Stop();
    });

    UpdateTerm(GetCurrentTerm() + 1, config.node_id);

    LOG("CANDIDATE: start new elections with term {}", GetCurrentTerm());

    size_t votes_count = 1;

    RequestVoteRequest request_vote;
    request_vote.set_term(GetCurrentTerm());
    request_vote.set_candidate_id(config.node_id);
    request_vote.set_last_log_term(GetLastSavedLogTerm());
    request_vote.set_last_log_index(GetSavedLogSize());

    std::vector<boost::fibers::fiber> requests;

    for (size_t node_id = 0; node_id < config.raft_nodes.size(); ++node_id) {
      if (node_id == config.node_id) {
        continue;
      }
      requests.emplace_back([&, node_id]() {
        LOG_DBG("CANDIDATE: start RequestVote to node {}", node_id);
        Result<RequestVoteResult, rt::rpc::RpcError> result =
            clients[node_id]->RequestVote(request_vote, stop_election.GetToken());

        if (result.HasError()) {
          LOG_ERR("CANDIDATE: finished RequestVote to node {} with error: {}", node_id,
                  result.GetError().Message());
          return;
        }

        if (GetCurrentTerm() < result.GetValue().term()) {
          LOG_DBG(
              "CANDIDATE: finished RequestVote to node {}, received term {}, change state to "
              "FOLLOWER",
              node_id, result.GetValue().term());
          UpdateTerm(result.GetValue().term());
          if (state != State::Follower) {
            BecomeFollower();
          }
          return;
        }

        if (!result.GetValue().vote_granted()) {
          LOG("CANDIDATE: node {} denied vote", node_id);
          return;
        }

        LOG("CANDIDATE: node {} granted vote", node_id);
        ++votes_count;
        if (votes_count == MajorityCount() && state == State::Candidate) {
          LOG_DBG("CANDIDATE: got majority of votes, become leader");
          BecomeLeader();
        }
      });
    }

    for (auto& request : requests) {
      request.join();
    }
    election_timer.join();
  }

  rt::Duration GetElectionTimeout() noexcept {
    return rt::GetRandomDuration(config.election_timeout);
  }

  size_t MajorityCount() const noexcept {
    return config.raft_nodes.size() / 2 + 1;
  }

  uint64_t GetLastSavedLogTerm() noexcept {
    uint64_t result = 0;
    auto iterator = raft_log->NewIterator();
    iterator.SeekToLast();
    if (iterator.Valid()) {
      result = iterator.GetValue().term();
    }
    return result;
  }

  uint64_t GetSavedLogSize() noexcept {
    uint64_t result = 0;
    auto iterator = raft_log->NewIterator();
    iterator.SeekToLast();
    if (iterator.Valid()) {
      result = iterator.GetKey();
    }
    return result;
  }

  LogEntry GetLogEntry(size_t index) noexcept {
    const size_t saved_log_size = GetSavedLogSize();
    VERIFY(index > 0 && index <= saved_log_size + pending_log.size(), "Invalid log index");
    if (index <= saved_log_size) {
      return raft_log->Get(index).GetValue();
    } else {
      return pending_log[index - saved_log_size - 1];
    }
  }

  uint64_t GetCurrentTerm() noexcept {
    return raft_state->Get("current_term").GetValue();
  }

  std::optional<uint64_t> GetVotedFor() noexcept {
    auto result = raft_state->Get("voted_for");
    if (result.HasError()) {
      return std::nullopt;
    }
    return result.GetValue();
  }

  void UpdateTerm(uint64_t term, std::optional<uint64_t> voted_for = {}) noexcept {
    VERIFY(term > GetCurrentTerm(), "Invalid update term");
    auto write_batch = raft_state->MakeWriteBatch();
    write_batch.Put("current_term", term).ExpectOk();
    if (voted_for) {
      write_batch.Put("voted_for", *voted_for).ExpectOk();
    } else {
      write_batch.Delete("voted_for").ExpectOk();
    }
    raft_state->Write(std::move(write_batch)).ExpectOk();
  }

  //////////////////////////////////////////////////////////

  rt::StopSource stop_election;
  rt::StopSource stop_leader;

  State state = State::Follower;

  RaftConfig config;
  std::vector<std::unique_ptr<rt::rpc::RaftInternalsClient>> clients;

  rt::StopSource reset_election_timeout;
  rt::StopSource reset_heartbeat_timeout;

  //////////////////////////////////////////////////////////
  // Persistent state
  //////////////////////////////////////////////////////////

  // Contains two keys: "current_term: and "voted_for"
  RaftStateDbPtr raft_state;
  // Key is log id
  RaftLogDbPtr raft_log;

  //////////////////////////////////////////////////////////
  // Volatile state
  //////////////////////////////////////////////////////////

  uint64_t commit_index = 0;

  //////////////////////////////////////////////////////////
  // Leaders volatile state
  //////////////////////////////////////////////////////////

  std::vector<uint64_t> match_index;
  std::vector<uint64_t> next_index;
  uint64_t current_term_start_index = 0;

  std::unordered_map<uint64_t, PendingRequest*> pending_requests;
  std::vector<LogEntry> pending_log;

  //////////////////////////////////////////////////////////
  // State machine
  //////////////////////////////////////////////////////////

  impl::ExactlyOnceStateMachine rsm;
};

std::string ToString(const std::vector<rt::Endpoint>& nodes) {
  std::string result;
  for (const auto& node : nodes) {
    result += node.ToString() + " ";
  }
  if (!result.empty()) {
    result.pop_back();
  }
  return result;
}

Status<std::string> RunMain(IStateMachine* state_machine, RaftConfig config) noexcept {
  rt::db::Options db_options{.create_if_missing = true};
  auto raft_state_db = rt::kv::Open(config.raft_state_db_path, db_options, rt::serde::StringSerde{},
                                    rt::serde::U64Serde{});
  if (raft_state_db.HasError()) {
    std::string message =
        fmt::format("cannot open state database at path {}: {}", config.raft_state_db_path,
                    raft_state_db.GetError().Message());
    LOG_CRIT(message);
    return Err(std::move(message));
  }

  auto raft_log_db = rt::kv::Open(config.log_db_path, db_options, rt::serde::U64Serde{},
                                  rt::serde::ProtobufSerde<LogEntry>{});
  if (raft_log_db.HasError()) {
    std::string message = fmt::format("cannot open log database at path {}: {}", config.log_db_path,
                                      raft_log_db.GetError().Message());
    LOG_CRIT(message);
    return Err(std::move(message));
  }

  RaftNode node(state_machine, config, std::move(raft_state_db.GetValue()),
                std::move(raft_log_db.GetValue()));

  LOG("Starting raft node, cluster: {}, node_id: {}", ToString(config.raft_nodes), config.node_id);

  rt::rpc::Server server;
  server.Register(static_cast<rt::rpc::RaftInternalsStub*>(&node));
  server.Register(static_cast<rt::rpc::RaftApiStub*>(&node));
  server.Start(config.raft_nodes[config.node_id].port);

  // Raft node does not support concurrent execution
  boost::fibers::fiber worker([&server]() {
    server.Run();
  });

  node.StartNode();

  server.ShutDown();
  worker.join();

  return Ok();
}

}  // namespace mtf::raft
