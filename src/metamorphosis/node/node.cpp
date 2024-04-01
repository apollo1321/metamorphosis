#include "node.h"

#include <google/protobuf/util/message_differencer.h>

#include <openssl/md5.h>

#include <metamorphosis/metamorphosis.client.h>
#include <metamorphosis/metamorphosis.service.h>

#include <runtime/api.h>
#include <runtime/util/event/event.h>
#include <runtime/util/hash/md5.h>
#include <runtime/util/serde/protobuf_serde.h>
#include <runtime/util/serde/string_serde.h>
#include <runtime/util/serde/u64_serde.h>

#include <util/condition_check.h>
#include <util/defer.h>

namespace mtf::mtf {

using StateDbPtr = rt::kv::KVStoragePtr<rt::serde::StringSerde, rt::serde::U64Serde>;
using LogDbPtr =
    rt::kv::KVStoragePtr<rt::serde::U64Serde, rt::serde::ProtobufSerde<AppendEntriesReq::LogEntry>>;

struct MetamorphosisNode final : public rt::rpc::MetamorphosisApiStub,
                                 public rt::rpc::MetamorphosisInternalsStub {
 public:
  enum class State {
    Leader,
    Follower,
    Candidate,
  };

  struct PendingRequest {
    rt::Event replication_finished{};
    uint64_t offset{};
  };

 public:
  explicit MetamorphosisNode(NodeConfig config, StateDbPtr state_db, LogDbPtr log_db)
      : config{config}, raft_state{std::move(state_db)}, raft_log{std::move(log_db)} {
    {
      auto result = raft_state->Get("current_term");
      if (result.HasError()) {
        raft_state->Put("current_term", 0).ExpectOk();
      }
    }

    for (size_t index = 0; index < config.cluster_nodes.size(); ++index) {
      if (index != config.node_id) {
        clients.emplace_back(
            std::make_unique<rt::rpc::MetamorphosisInternalsClient>(config.cluster_nodes[index]));
      } else {
        clients.emplace_back(nullptr);
      }
    }

    for (uint64_t index = 1; index <= GetSavedLogSize(); ++index) {
      ++producer_next_sequence_id[raft_log->Get(index).GetValue().producer_id()];
    }
  }

  //////////////////////////////////////////////////////////
  // API
  //////////////////////////////////////////////////////////

  Result<AppendRsp, rt::rpc::RpcError> Append(const AppendReq& request) noexcept override {
    AppendRsp response;

    if (state != State::Leader) {
      LOG("APPEND: fail: not a leader");
      response.set_status(AppendRsp_Status_NotALeader);
      return Ok(std::move(response));
    }

    if (pending_producers.contains(request.producer_id())) {
      LOG("APPEND: fail: parallel request from producer");
      response.set_status(AppendRsp_Status_ParallelRequest);
      return Ok(std::move(response));
    }
    pending_producers.insert(request.producer_id());
    DEFER {
      pending_producers.erase(request.producer_id());
    };

    if (request.sequence_id() != producer_next_sequence_id[request.producer_id()]) {
      LOG("APPEND: invalid sequence id");
      response.set_status(AppendRsp_Status_InvalidSequenceId);
      response.set_next_sequence_id(producer_next_sequence_id[request.producer_id()]);
      return Ok(std::move(response));
    }

    auto md5hash = rt::ComputeMd5Hash(request.data());
    LOG("APPEND: MD5 hash: {}", ToString(md5hash));

    uint64_t new_log_index = next_index[config.node_id]++;
    uint64_t current_term = GetCurrentTerm();

    {
      AppendEntriesReq::LogEntry entry;
      entry.set_term(current_term);
      entry.set_producer_id(request.producer_id());
      entry.set_data(request.data());
      entry.set_md5hash(md5hash.digest.data(), md5hash.digest.size());

      pending_log.emplace_back(std::move(entry));
    }

    LOG("APPEND: append command to log, index = {}", new_log_index);

    PendingRequest pending_request;

    rt::StopCallback finish_guard(stop_leader.GetToken(), [&pending_request]() {
      pending_request.replication_finished.Signal();
    });

    pending_requests[new_log_index] = &pending_request;

    reset_heartbeat_timeout.Stop();
    reset_heartbeat_timeout = rt::StopSource();

    LOG("APPEND: await replicating message to replicas");
    pending_request.replication_finished.Await();

    pending_requests.erase(new_log_index);

    if (current_term != GetCurrentTerm()) {
      response.set_status(AppendRsp_Status_NotALeader);
      LOG("APPEND: fail: current replica is not a leader");
      return Ok(std::move(response));
    }

    VERIFY(state == State::Leader, "Invalid session state");

    LOG("APPEND: success");
    response.set_status(AppendRsp_Status_Commited);
    response.set_offset(pending_request.offset);
    response.set_next_sequence_id(producer_next_sequence_id[request.producer_id()]);

    return Ok(std::move(response));
  }

  Result<ReadRsp, rt::rpc::RpcError> Read(const ReadReq& request) noexcept override {
    ReadRsp response;

    LOG("READ: offset = {}, commit_index = {}", request.offset(), commit_index);

    if (request.offset() > commit_index) {
      LOG("READ: invalid offset: not commited");
      response.set_status(ReadRsp_Status_NOT_FOUND);
    } else {
      auto result = raft_log->Get(request.offset());

      if (result.HasError()) {
        LOG("READ: error = ", result.GetError().Message());
        return Err(rt::rpc::RpcError(rt::rpc::RpcErrorType::Internal, result.GetError().Message()));
      } else {
        auto& value = result.GetValue();
        if (!value.data().empty()) {
          LOG("READ: ok, full message");
          response.set_status(ReadRsp_Status_FULL_MESSAGE);
          response.set_data(value.data());
        } else {
          LOG("READ: ok, hash only");
          response.set_status(ReadRsp_Status_HASH_ONLY);
          response.set_md5hash(value.md5hash());
        }
      }
    }

    return Ok(std::move(response));
  }

  //////////////////////////////////////////////////////////
  // Internal RPC's
  //////////////////////////////////////////////////////////

  Result<AppendEntriesRsp, rt::rpc::RpcError> AppendEntries(
      const AppendEntriesReq& request) noexcept override {
    AppendEntriesRsp response;

    response.set_term(GetCurrentTerm());
    response.set_success(false);

    if (request.entries_size() > 0) {
      LOG("APPEND_ENTRIES: current_term = {}, request_term = {}, log_size = {}", GetCurrentTerm(),
          request.term(), request.entries_size());
    }

    if (request.term() < GetCurrentTerm()) {
      LOG("APPEND_ENTRIES: dismiss, request_term < current_term");
      return Ok(std::move(response));
    }

    reset_election_timeout.Stop();

    if (request.term() >= GetCurrentTerm() && state != State::Follower) {
      LOG("APPEND_ENTRIES: request_term >= current_term; become to FOLLOWER");
      BecomeFollower();
    }

    if (request.term() > GetCurrentTerm()) {
      UpdateTerm(request.term());
    }

    VERIFY(request.prev_log_index() != 0 || request.prev_log_term() == 0, "invalid prev_log_term");

    if (request.prev_log_index() > GetSavedLogSize()) {
      LOG("APPEND_ENTRIES: dismiss: prev_log_index = {} > log_size = {}", request.prev_log_index(),
          GetSavedLogSize());
      return Ok(std::move(response));
    }

    if (request.prev_log_index() != 0) {
      const uint64_t log_term = raft_log->Get(request.prev_log_index()).GetValue().term();
      if (log_term != request.prev_log_term()) {
        LOG("APPEND_ENTRIES: dismiss: prev_log_term = {} != request_prev_log_term = {}", log_term,
            request.prev_log_term());
        return Ok(std::move(response));
      }
    }

    if (request.entries_size() > 0) {
      LOG("APPEND_ENTRIES: accept, writting entries to local log");
    }
    response.set_success(true);

    // Check that if leader tries to overwrite the commited log, it must be the same.
    // The situation when the leader overwrites commited log may occur in case of rpc retries.
    for (uint64_t index = request.prev_log_index() + 1;
         index <= commit_index && index - request.prev_log_index() - 1 < request.entries_size();
         ++index) {
      using google::protobuf::util::MessageDifferencer;

      AppendEntriesReq::LogEntry entry_from_log = raft_log->Get(index).GetValue();
      AppendEntriesReq::LogEntry entry_from_request =
          request.entries()[index - request.prev_log_index() - 1];

      VERIFY(MessageDifferencer::Equals(entry_from_request, entry_from_log),
             "invalid state: committed log is inconsistent");
    }

    for (uint64_t index = request.prev_log_index() + 1; index <= GetSavedLogSize(); ++index) {
      uint64_t producer_id = raft_log->Get(index).GetValue().producer_id();
      --producer_next_sequence_id[producer_id];
      VERIFY(producer_next_sequence_id[producer_id] >= 0,
             "Producer next sequence id must be non-negative");
    }

    auto write_batch = raft_log->MakeWriteBatch();
    write_batch
        .DeleteRange(std::max(request.prev_log_index(), commit_index) + 1,
                     std::numeric_limits<uint64_t>::max())
        .ExpectOk();
    for (uint64_t index = std::max(request.prev_log_index(), commit_index) + 1;
         index - request.prev_log_index() - 1 < request.entries_size(); ++index) {
      auto& log_entry = request.entries()[index - request.prev_log_index() - 1];
      write_batch.Put(index, log_entry).ExpectOk();
      ++producer_next_sequence_id[log_entry.producer_id()];
      if (log_entry.data().empty()) {
        LOG("APPEND_ENTRIES: log entry {} has partial message", index);
      } else {
        LOG("APPEND_ENTRIES: log entry {} has full message", index);
      }
    }
    raft_log->Write(std::move(write_batch)).ExpectOk();

    const uint64_t new_commit_index =
        std::max(commit_index, std::min<uint64_t>(request.leader_commit(), GetSavedLogSize()));

    if (commit_index != new_commit_index) {
      LOG("APPEND_ENTRIES: updating commit index, prev = {}, new = {}", commit_index,
          new_commit_index);
    }

    commit_index = new_commit_index;

    return Ok(std::move(response));
  }

  Result<RequestVoteRsp, rt::rpc::RpcError> RequestVote(
      const RequestVoteReq& request) noexcept override {
    RequestVoteRsp response;

    LOG("REQUEST_VOTE: start, current_term = {}, candidate_id = {}, request_term = {}",
        GetCurrentTerm(), request.candidate_id(), request.term());

    response.set_term(GetCurrentTerm());

    if (request.term() < GetCurrentTerm()) {
      LOG("REQUEST_VOTE: dismiss: request_term < current_term");
      response.set_vote_granted(false);
      return Ok(std::move(response));
    }

    if (request.term() > response.term()) {
      LOG("REQUEST_VOTE: request_term > current_term; change to FOLLOWER");
      UpdateTerm(request.term());
      if (state != State::Follower) {
        BecomeFollower();
      }
    }

    const auto voted_for = GetVotedFor();

    if (voted_for == request.candidate_id()) {
      LOG("REQUEST_VOTE: accept: voted_for == candidate_id");
      response.set_vote_granted(true);
      return Ok(std::move(response));
    }

    if (voted_for) {
      LOG("REQUEST_VOTE: dismiss: already voted_for = {}", *voted_for);
      response.set_vote_granted(false);
      return Ok(std::move(response));
    }

    const uint64_t last_log_term = GetLastSavedLogTerm();
    if (last_log_term > request.last_log_term()) {
      LOG("REQUEST_VOTE: dismiss: (current last_log_term = {}) > (candidate last_log_term = {})",
          last_log_term, request.last_log_term());
      response.set_vote_granted(false);
    } else if (last_log_term == request.last_log_term()) {
      LOG("REQUEST_VOTE: (current last_log_term = {}) == (candidate last_log_term = {})",
          last_log_term, request.last_log_term());
      if (GetSavedLogSize() <= request.last_log_index()) {
        LOG("REQUEST_VOTE: accept: (current log_size = {}) <= (candidate log_size = {})",
            GetSavedLogSize(), request.last_log_index());
        response.set_vote_granted(true);
      } else {
        LOG("REQUEST_VOTE: dismiss: (current log_size = {}) > (candidate log_size {})",
            GetSavedLogSize(), request.last_log_index());
        response.set_vote_granted(false);
      }
    } else {
      LOG("REQUEST_VOTE: accept: (current last_log_term = {}) < (candidate last_log_term = {})",
          last_log_term, request.last_log_term());
      response.set_vote_granted(true);
    }
    if (response.vote_granted()) {
      raft_state->Put("voted_for", request.candidate_id()).ExpectOk();
    }
    return Ok(std::move(response));
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
    next_index = std::vector<uint64_t>(config.cluster_nodes.size(), GetSavedLogSize() + 1);
    match_index = std::vector<uint64_t>(config.cluster_nodes.size(), 0);
    reset_heartbeat_timeout = rt::StopSource();
    stop_leader = rt::StopSource();
    stop_election.Stop();
    alive_main_nodes_count = MajorityCount();
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

  void StartLeader() noexcept {
    std::vector<boost::fibers::fiber> sessions;

    for (size_t node_id = 0; node_id < config.cluster_nodes.size(); ++node_id) {
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
        const auto& log_entry = pending_log[index];
        write_batch.Put(match_index[config.node_id] + index + 1, log_entry).ExpectOk();
        ++producer_next_sequence_id[log_entry.producer_id()];
        pending_producers.erase(log_entry.producer_id());
      }
      // It is important to note that writes are synchronous.
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
    bool is_alive = true;

    LOG("LEADER[{}]: is main node: {}", node_id, IsMainNode(node_id));

    while (state == State::Leader) {
      VERIFY(next_index[node_id] >= 1, "invalid next_index");
      AppendEntriesReq request;

      request.set_term(GetCurrentTerm());
      request.set_leader_id(config.node_id);
      request.set_prev_log_index(next_index[node_id] - 1);
      request.set_prev_log_term(
          next_index[node_id] == 1 ? 0 : raft_log->Get(next_index[node_id] - 1).GetValue().term());
      request.set_leader_commit(commit_index);

      uint64_t last_log_entry_index{};
      if (!IsMainNode(node_id) && alive_main_nodes_count == MajorityCount()) {
        last_log_entry_index = commit_index;
      } else {
        last_log_entry_index = next_index[config.node_id] - 1;
      }

      for (size_t index = next_index[node_id]; index <= last_log_entry_index; ++index) {
        auto log_entry = GetLogEntry(index);
        if (index <= commit_index) {
          // If it is known that the log entry is commited, we can send only hash to the follower.
          log_entry.mutable_data()->clear();
        }
        *request.add_entries() = std::move(log_entry);
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

      if (request.entries_size() > 0) {
        LOG("LEADER[{}]: start AppendEntries with next_index = {}, log_size = {}", node_id,
            next_index[node_id], request.entries_size());
      }
      auto result = clients[node_id]->AppendEntries(request, request_stop.GetToken());

      if (state != State::Leader) {
        LOG_ERR("LEADER[{}]: Not a leader", node_id);
        continue;
      }

      if (result.HasError()) {
        if (is_alive) {
          LOG_ERR("LEADER[{}]: AppendEntries RPC has failed with error: {}", node_id,
                  result.GetError().Message());
          is_alive = false;
          if (IsMainNode(node_id)) {
            --alive_main_nodes_count;
            LOG("LEADER[{}]: alive main node count has decreased: {}", node_id,
                alive_main_nodes_count);
          }
        }
        request_timeout.join();  // Sleep for some time
        continue;
      }

      if (!is_alive) {
        is_alive = true;
        if (IsMainNode(node_id)) {
          ++alive_main_nodes_count;
          LOG("LEADER[{}]: main node become alive: {}", node_id, alive_main_nodes_count);
        } else {
          LOG("LEADER[{}]: node become alive", node_id);
        }
      }

      AppendEntriesRsp& response = result.GetValue();
      if (response.term() > GetCurrentTerm()) {
        LOG("LEADER[{}]: received greater term = {}, change state to FOLLOWER", node_id,
            response.term());
        UpdateTerm(response.term());
        if (state != State::Follower) {
          BecomeFollower();
        }
        continue;
      }

      if (!response.success()) {
        LOG("LEADER[{}]: log is inconsistent, decrement next_index", node_id);
        VERIFY(next_index[node_id] >= 2, "invalid state");
        --next_index[node_id];
        continue;
      }

      match_index[node_id] = request.prev_log_index() + request.entries_size();
      next_index[node_id] += request.entries_size();
      if (request.entries_size() > 0) {
        LOG("LEADER[{}]: success, update match_index = {} and next_index = {}", node_id,
            match_index[node_id], next_index[node_id]);
      }

      UpdateCommitIndex();

      if (next_index[node_id] == next_index[config.node_id]) {
        if (request.entries_size() > 0) {
          LOG("LEADER[{}]: all log replicated, wait on heart_beat_timeout", node_id);
        }

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
      for (size_t node_id = 0; node_id < config.cluster_nodes.size(); ++node_id) {
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
      auto it = pending_requests.find(index);
      if (it != pending_requests.end()) {
        it->second->offset = index;
        it->second->replication_finished.Signal();
      }
    }

    commit_index = new_commit_index;
  }

  void StartFollower() noexcept {
    reset_election_timeout = rt::StopSource();
    if (!rt::SleepFor(GetElectionTimeout(), reset_election_timeout.GetToken())) {
      LOG("FOLLOWER: election timeout expired");
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

    RequestVoteReq request_vote;
    request_vote.set_term(GetCurrentTerm());
    request_vote.set_candidate_id(config.node_id);
    request_vote.set_last_log_term(GetLastSavedLogTerm());
    request_vote.set_last_log_index(GetSavedLogSize());

    std::vector<boost::fibers::fiber> requests;

    for (size_t node_id = 0; node_id < config.cluster_nodes.size(); ++node_id) {
      if (node_id == config.node_id) {
        continue;
      }
      requests.emplace_back([&, node_id]() {
        LOG("CANDIDATE: start RequestVote to node {}", node_id);
        auto result = clients[node_id]->RequestVote(request_vote, stop_election.GetToken());

        if (result.HasError()) {
          LOG_ERR("CANDIDATE: finished RequestVote to node {} with error: {}", node_id,
                  result.GetError().Message());
          return;
        }

        if (GetCurrentTerm() < result.GetValue().term()) {
          LOG("CANDIDATE: finished RequestVote to node {}, received term {}, change state to "
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
          LOG("CANDIDATE: got majority of votes, become leader");
          BecomeLeader();
        }
      });
    }

    for (auto& request : requests) {
      request.join();
    }
    election_timer.join();
  }

  //////////////////////////////////////////////////////////

  uint64_t GetCurrentTerm() noexcept {
    return raft_state->Get("current_term").GetValue();
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

  uint64_t GetSavedLogSize() noexcept {
    uint64_t result = 0;
    auto iterator = raft_log->NewIterator();
    iterator.SeekToLast();
    if (iterator.Valid()) {
      result = iterator.GetKey();
    }
    return result;
  }

  std::optional<uint64_t> GetVotedFor() noexcept {
    auto result = raft_state->Get("voted_for");
    if (result.HasError()) {
      return std::nullopt;
    }
    return result.GetValue();
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

  AppendEntriesReq::LogEntry GetLogEntry(size_t index) noexcept {
    const size_t saved_log_size = GetSavedLogSize();
    VERIFY(index > 0 && index <= saved_log_size + pending_log.size(), "Invalid log index");
    if (index <= saved_log_size) {
      return raft_log->Get(index).GetValue();
    } else {
      return pending_log[index - saved_log_size - 1];
    }
  }

  size_t MajorityCount() const noexcept {
    return config.cluster_nodes.size() / 2 + 1;
  }

  rt::Duration GetElectionTimeout() noexcept {
    return rt::GetRandomDuration(config.election_timeout);
  }

  bool IsMainNode(size_t index) noexcept {
    size_t majority_count = MajorityCount();

    if (config.node_id >= majority_count) {
      --majority_count;
    }

    return index < majority_count;
  }

  //////////////////////////////////////////////////////////

  rt::StopSource stop_election;
  rt::StopSource stop_leader;

  State state = State::Follower;

  NodeConfig config;
  std::vector<std::unique_ptr<rt::rpc::MetamorphosisInternalsClient>> clients;

  rt::StopSource reset_election_timeout;
  rt::StopSource reset_heartbeat_timeout;

  //////////////////////////////////////////////////////////
  // Persistent state
  //////////////////////////////////////////////////////////

  // Raft State storage contains two keys: "current_term" and "voted_for".
  StateDbPtr raft_state;

  // Raft Log storage contains LogEntries that are indexed by log id.
  LogDbPtr raft_log;

  //////////////////////////////////////////////////////////
  // Volatile state
  //////////////////////////////////////////////////////////

  uint64_t commit_index = 0;

  // The number of messages written into raft_log for a particular producer.
  std::unordered_map<uint64_t, uint64_t> producer_next_sequence_id;

  std::unordered_set<uint64_t> pending_producers;

  //////////////////////////////////////////////////////////
  // Leaders volatile state
  //////////////////////////////////////////////////////////

  std::vector<uint64_t> match_index;
  std::vector<uint64_t> next_index;
  uint64_t current_term_start_index = 0;

  std::unordered_map<uint64_t, PendingRequest*> pending_requests;
  std::vector<AppendEntriesReq::LogEntry> pending_log;

  size_t alive_main_nodes_count{};
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

Status<std::string> RunMain(NodeConfig config) noexcept {
  rt::db::Options db_options{.create_if_missing = true};
  auto state_db = rt::kv::Open(config.state_db_path, db_options, rt::serde::StringSerde{},
                               rt::serde::U64Serde{});
  if (state_db.HasError()) {
    std::string message = fmt::format("cannot open state database at path {}: {}",
                                      config.state_db_path, state_db.GetError().Message());
    LOG_CRIT(message);
    return Err(std::move(message));
  }

  rt::GetLogger()->set_level(spdlog::level::debug);

  auto log_db = rt::kv::Open(config.log_db_path, db_options, rt::serde::U64Serde{},
                             rt::serde::ProtobufSerde<AppendEntriesReq::LogEntry>{});
  if (log_db.HasError()) {
    std::string message = fmt::format("cannot open log database at path {}: {}", config.log_db_path,
                                      log_db.GetError().Message());
    LOG_CRIT(message);
    return Err(std::move(message));
  }

  MetamorphosisNode node(config, std::move(state_db.GetValue()), std::move(log_db.GetValue()));

  LOG("Starting raft node, cluster: {}, node_id: {}", ToString(config.cluster_nodes),
      config.node_id);

  rt::rpc::Server server;
  server.Register(static_cast<rt::rpc::MetamorphosisInternalsStub*>(&node));
  server.Register(static_cast<rt::rpc::MetamorphosisApiStub*>(&node));
  server.Start(config.cluster_nodes[config.node_id].port);

  // Metamorphosis node does not support concurrent execution.
  boost::fibers::fiber worker([&server]() {
    server.Run();
  });

  node.StartNode();

  server.ShutDown();
  worker.join();

  return Ok();
}

}  // namespace mtf::mtf
