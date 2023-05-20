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

namespace ceq::raft {

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
    LOG("EXECUTE: start");

    Response response;
    if (state != State::Leader) {
      LOG("EXECUTE: fail: not a leader");
      response.set_status(Response_Status_NotALeader);
      return Ok(std::move(response));
    }

    uint64_t new_log_index = GetLogSize() + 1;
    uint64_t current_term = GetCurrentTerm();

    {
      LogEntry entry;
      entry.set_term(current_term);
      *entry.mutable_request() = request;
      raft_log->Put(new_log_index, entry).ExpectOk();
    }

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

    if (current_term != GetCurrentTerm()) {
      response.set_status(Response_Status_NotALeader);
      LOG("EXECUTE: fail: current replica is not a leader");
      return Ok(std::move(response));
    }

    LOG("EXECUTE: success");
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

    LOG("APPEND_ENTRIES: start, current_term = {}, request_term = {}", result.term(),
        request.term());

    if (request.term() < result.term()) {
      LOG("APPEND_ENTRIES: dismiss, request_term < current_term");
      result.set_success(false);
      return Ok(std::move(result));
    }

    reset_election_timeout.Stop();

    if (request.term() >= result.term()) {
      if (state != State::Follower) {
        LOG("APPEND_ENTRIES: request_term >= current_term; change to FOLLOWER");
        state = State::Follower;
        stop_election.Stop();
        stop_leader.Stop();
      }
      if (request.term() > result.term()) {
        auto write_batch = raft_state->MakeWriteBatch();
        write_batch.Put("current_term", request.term()).ExpectOk();
        write_batch.Delete("voted_for").ExpectOk();
        raft_state->Write(std::move(write_batch)).ExpectOk();
      }
    }

    VERIFY(request.prev_log_index() != 0 || request.prev_log_term() == 0, "invalid prev_log_term");

    if (request.prev_log_index() > GetLogSize()) {
      LOG("APPEND_ENTRIES: dismiss: prev_log_index = {} > log_size = {}", request.prev_log_index(),
          GetLogSize());
      result.set_success(false);
      return Ok(std::move(result));
    }

    if (request.prev_log_index() != 0) {
      const uint64_t log_term = raft_log->Get(request.prev_log_index()).GetValue().term();
      if (log_term != request.prev_log_term()) {
        LOG("APPEND_ENTRIES: dismiss: prev_log_term = {} != request_prev_log_term = {}", log_term,
            request.prev_log_term());
        result.set_success(false);
        return Ok(std::move(result));
      }
    }

    LOG("APPEND_ENTRIES: accept, writting entries to local log");
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
        std::max(commit_index, std::min<uint64_t>(request.leader_commit(), GetLogSize()));

    if (commit_index != new_commit_index) {
      LOG("APPEND_ENTRIES: updating commit index, prev = {}, new = {}", commit_index,
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
    if (request.term() < result.term()) {
      LOG("REQUEST_VOTE: dismiss: request_term < current_term");
      result.set_vote_granted(false);
      return Ok(std::move(result));
    }

    if (request.term() > result.term()) {
      LOG("REQUEST_VOTE: request_term > current_term; change to FOLLOWER");
      auto write_batch = raft_state->MakeWriteBatch();
      write_batch.Put("current_term", request.term()).ExpectOk();
      write_batch.Delete("voted_for").ExpectOk();
      raft_state->Write(std::move(write_batch)).ExpectOk();

      state = State::Follower;
      reset_election_timeout.Stop();
      stop_election.Stop();
      stop_leader.Stop();
    }

    const auto voted_for = GetVotedFor();

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
      if (GetLogSize() <= request.last_log_index()) {
        LOG("REQUEST_VOTE: accept: current log_size = {} <= candidate log_size {}", GetLogSize(),
            request.last_log_index());
        result.set_vote_granted(true);
      } else {
        LOG("REQUEST_VOTE: dismiss: current log_size = {} > candidate log_size {}", GetLogSize(),
            request.last_log_index());
        result.set_vote_granted(false);
      }
    } else {
      LOG("REQUEST_VOTE: accept: current last_log_term = {} < candidate last_log_term = {}",
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

  void StartLeader() noexcept {
    LOG("LEADER: start with term {}", GetCurrentTerm());
    DEFER {
      LOG("LEADER: finished");
    };

    next_index = std::vector<uint64_t>(config.raft_nodes.size(), GetLogSize() + 1);
    match_index = std::vector<uint64_t>(config.raft_nodes.size(), 0);

    std::vector<boost::fibers::fiber> sessions;

    stop_leader = rt::StopSource();

    for (size_t node_id = 0; node_id < config.raft_nodes.size(); ++node_id) {
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

      request.set_term(GetCurrentTerm());
      request.set_leader_id(config.node_id);
      request.set_prev_log_index(next_index[node_id] - 1);
      request.set_prev_log_term(
          next_index[node_id] == 1 ? 0 : raft_log->Get(next_index[node_id] - 1).GetValue().term());
      request.set_leader_commit(commit_index);

      {
        const uint64_t log_size = GetLogSize();
        for (size_t index = next_index[node_id]; index <= log_size; ++index) {
          *request.add_entries() = raft_log->Get(index).GetValue();
        }
      }

      rt::StopSource request_stop;
      rt::StopCallback request_stop_propagate(stop_leader.GetToken(), [&]() {
        request_stop.Stop();
      });

      boost::fibers::fiber request_timeout([&]() {
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
      LOG("LEADER[{}]: finished AppendEntries", node_id);

      if (state != State::Leader) {
        LOG("LEADER[{}]: not a leader", node_id);
        continue;
      }

      if (result.HasError()) {
        LOG("LEADER[{}]: AppendEntries end with error: {}", node_id, result.GetError().Message());
        request_timeout.join();  // Sleep for some time
        continue;
      }

      AppendEntriesResult& response = result.GetValue();
      if (response.term() > GetCurrentTerm()) {
        LOG("LEADER[{}]: received greater term = {}, change state to FOLLOWER", node_id,
            response.term());
        auto write_batch = raft_state->MakeWriteBatch();
        write_batch.Put("current_term", response.term()).ExpectOk();
        write_batch.Delete("voted_for").ExpectOk();
        raft_state->Write(std::move(write_batch)).ExpectOk();
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

      if (next_index[node_id] == GetLogSize() + 1) {
        LOG("LEADER[{}]: all log replicated, wait on heart_beat_timeout", node_id);

        reset_heartbeat_timeout = rt::StopSource();

        rt::StopCallback heart_beat_stop_propagate(stop_leader.GetToken(), [&]() {
          reset_heartbeat_timeout.Stop();
        });

        rt::SleepFor(config.heart_beat_period, reset_heartbeat_timeout.GetToken());
      }
    }
  }

  void UpdateCommitIndex() noexcept {
    VERIFY(state == State::Leader, "UpdateCommitIndex must be called only by leaders");

    uint64_t new_commit_index = commit_index;

    for (uint64_t index = GetLogSize(); index > commit_index && index >= current_term_start_index;
         --index) {
      size_t match_count = 1;

      for (size_t node_id = 0; node_id < config.raft_nodes.size(); ++node_id) {
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

    if (commit_index != new_commit_index) {
      LOG("LEADER: updating commit index, prev = {}, new = {}", commit_index, new_commit_index);
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
    LOG("FOLLOWER: start with term {}", GetCurrentTerm());
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
      auto write_batch = raft_state->MakeWriteBatch();
      write_batch.Put("current_term", GetCurrentTerm() + 1).ExpectOk();
      write_batch.Put("voted_for", config.node_id).ExpectOk();
      raft_state->Write(std::move(write_batch)).ExpectOk();

      LOG("CANDIDATE: start new elections with term {}", GetCurrentTerm());

      size_t votes_count = 1;

      RequestVoteRequest request_vote;
      request_vote.set_term(GetCurrentTerm());
      request_vote.set_candidate_id(config.node_id);
      request_vote.set_last_log_term(GetLastLogTerm());
      request_vote.set_last_log_index(GetLogSize());

      stop_election = rt::StopSource{};

      boost::fibers::fiber election_timer([&]() {
        rt::SleepFor(GetElectionTimeout(), stop_election.GetToken());
        stop_election.Stop();
      });

      std::vector<boost::fibers::fiber> requests;

      for (size_t node_id = 0; node_id < config.raft_nodes.size(); ++node_id) {
        if (node_id == config.node_id) {
          continue;
        }
        requests.emplace_back([&, node_id]() {
          LOG("CANDIDATE: start RequestVote to node {}", node_id);
          Result<RequestVoteResult, rt::rpc::RpcError> result =
              clients[node_id]->RequestVote(request_vote, stop_election.GetToken());

          if (result.HasError()) {
            LOG("CANDIDATE: finished RequestVote to node {} with error: {}", node_id,
                result.GetError().Message());
            return;
          }

          if (state != State::Candidate) {
            LOG("CANDIDATE: finished RequestVote to node {}, not a CANDIDATE", node_id);
            return;
          }

          if (GetCurrentTerm() < result.GetValue().term()) {
            LOG("CANDIDATE: finished RequestVote to node {}, received term {}, change state to "
                "FOLLOWER",
                node_id, result.GetValue().term());
            auto write_batch = raft_state->MakeWriteBatch();
            write_batch.Put("current_term", result.GetValue().term()).ExpectOk();
            write_batch.Delete("voted_for").ExpectOk();
            raft_state->Write(std::move(write_batch)).ExpectOk();
            state = State::Follower;
            stop_election.Stop();
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
            current_term_start_index = GetLogSize() + 1;
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
    return rt::GetRandomDuration(config.election_timeout);
  }

  size_t MajorityCount() const noexcept {
    return config.raft_nodes.size() / 2 + 1;
  }

  uint64_t GetLastLogTerm() noexcept {
    uint64_t result = 0;
    auto iterator = raft_log->NewIterator();
    iterator.SeekToLast();
    if (iterator.Valid()) {
      result = iterator.GetValue().term();
    }
    return result;
  }

  uint64_t GetLogSize() noexcept {
    uint64_t result = 0;
    auto iterator = raft_log->NewIterator();
    iterator.SeekToLast();
    if (iterator.Valid()) {
      result = iterator.GetKey();
    }
    return result;
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

  //////////////////////////////////////////////////////////

  rt::StopSource stop_election;
  rt::StopSource stop_leader;

  State state = State::Follower;

  RaftConfig config;
  std::vector<std::unique_ptr<rt::rpc::RaftInternalsClient>> clients;

  rt::StopSource reset_election_timeout;
  rt::StopSource reset_heartbeat_timeout;

  std::unordered_map<uint64_t, PendingRequest*> pending_requests;

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

  //////////////////////////////////////////////////////////
  // State machine
  //////////////////////////////////////////////////////////

  impl::ExactlyOnceStateMachine rsm;
};

Status<std::string> RunMain(IStateMachine* state_machine, RaftConfig config) noexcept {
  rt::db::Options db_options{.create_if_missing = true};
  auto raft_state_db = rt::kv::Open(config.raft_state_db_path, db_options, rt::serde::StringSerde{},
                                    rt::serde::U64Serde{});
  if (raft_state_db.HasError()) {
    std::string message =
        fmt::format("cannot open state database at path {}: {}", config.raft_state_db_path,
                    raft_state_db.GetError().Message());
    LOG_CRITICAL(message);
    return Err(std::move(message));
  }

  auto raft_log_db = rt::kv::Open(config.log_db_path, db_options, rt::serde::U64Serde{},
                                  rt::serde::ProtobufSerde<LogEntry>{});
  if (raft_log_db.HasError()) {
    std::string message = fmt::format("cannot open log database at path {}: {}", config.log_db_path,
                                      raft_log_db.GetError().Message());
    LOG_CRITICAL(message);
    return Err(std::move(message));
  }

  RaftNode node(state_machine, config, std::move(raft_state_db.GetValue()),
                std::move(raft_log_db.GetValue()));

  rt::rpc::Server server;
  server.Register(static_cast<rt::rpc::RaftInternalsStub*>(&node));
  server.Register(static_cast<rt::rpc::RaftApiStub*>(&node));
  server.Start(config.raft_nodes[config.node_id].port);

  // Raft node does not support concurrent execution
  boost::fibers::fiber worker([&]() {
    server.Run();
  });

  node.StartNode();

  server.ShutDown();
  worker.join();

  return Ok();
}

}  // namespace ceq::raft
