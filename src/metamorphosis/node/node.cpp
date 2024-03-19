#include "node.h"

#include <metamorphosis/metamorphosis.client.h>
#include <metamorphosis/metamorphosis.service.h>

#include <runtime/api.h>
#include <runtime/util/event/event.h>
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
  enum class State {
    Leader,
    Follower,
    Candidate,
  };

  struct PendingRequest {
    rt::Event replication_finished{};
    google::protobuf::Any result{};
  };

  explicit MetamorphosisNode(NodeConfig config, StateDbPtr state_db, LogDbPtr log_db)
      : config{config}, mtf_state{std::move(state_db)}, mtf_queue{std::move(log_db)} {
    {
      // Setup current_term
      auto result = mtf_state->Get("current_term");
      if (result.HasError()) {
        mtf_state->Put("current_term", 0).ExpectOk();
      }
    }

    for (auto& endpoint : config.cluster_nodes) {
      clients.emplace_back(std::make_unique<rt::rpc::MetamorphosisInternalsClient>(endpoint));
    }
  }

  //////////////////////////////////////////////////////////
  // API
  //////////////////////////////////////////////////////////

  Result<AppendRsp, rt::rpc::RpcError> Append(const AppendReq& request) noexcept override {
    AppendRsp response;

    if (state != State::Leader) {
      LOG_DBG("EXECUTE: fail: not a leader");
      response.set_status(AppendRsp_Status_NotALeader);
      return Ok(std::move(response));
    }

    uint64_t new_log_index = next_index[config.node_id]++;
    uint64_t current_term = GetCurrentTerm();

    {
      AppendEntriesReq::LogEntry entry;
      entry.set_term(current_term);

      request.client_id();
      request.request_id();

      *entry.mutable_data() = request.data();
      pending_log.emplace_back(entry);
    }

    // LOG_DBG("EXECUTE: append command to log, index = {}", new_log_index);
    //
    // PendingRequest pending_request;
    //
    // rt::StopCallback finish_guard(stop_leader.GetToken(), [&pending_request]() {
    //   pending_request.replication_finished.Signal();
    // });
    //
    // pending_requests[new_log_index] = &pending_request;
    //
    // reset_heartbeat_timeout.Stop();
    // reset_heartbeat_timeout = rt::StopSource();
    //
    // LOG_DBG("EXECUTE: await replicating command to replicas");
    // pending_request.replication_finished.Await();
    //
    // pending_requests.erase(new_log_index);
    //
    // if (current_term != GetCurrentTerm()) {
    //   response.set_status(Response_Status_NotALeader);
    //   LOG_DBG("EXECUTE: fail: current replica is not a leader");
    //   return Ok(std::move(response));
    // }
    //
    // VERIFY(state == State::Leader, "Invalid session state");
    //
    // LOG_DBG("EXECUTE: success");
    // response.set_status(Response_Status_Commited);
    // *response.mutable_result() = std::move(pending_request.result);
    //
    return Ok(std::move(response));
  }

  Result<ReadRsp, rt::rpc::RpcError> Read(const ReadReq& request) noexcept override {
    ReadRsp response;

    if (next_index[config.node_id] >= request.message_id()) {
      response.set_status(ReadRsp_Status_NOT_FOUND);
    } else {
      auto result = mtf_queue->Get(request.message_id());

      if (result.HasError()) {
        if (result.GetError().error_type != rt::db::DBErrorType::NotFound) {
          return Err(result.GetError().Message());
        } else {
          response.set_status(ReadRsp_Status_TRUNCATED);
        }
      } else {
        auto& value = result.GetValue();
        if (value.has_data()) {
          response.set_status(ReadRsp_Status_FULL_MESSAGE);
          response.set_data(value.data());
        } else {
          response.set_status(ReadRsp_Status_HASH_ONLY);
          response.set_hash(value.hash());
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
    AppendEntriesRsp result;
    // result.set_term(GetCurrentTerm());
    // result.set_success(false);
    //
    // LOG("APPEND_ENTRIES: start, current_term = {}, request_term = {}, log_size = {}",
    //     GetCurrentTerm(), request.term(), request.entries_size());
    //
    // if (request.term() < GetCurrentTerm()) {
    //   LOG_DBG("APPEND_ENTRIES: dismiss, request_term < current_term");
    //   return Ok(std::move(result));
    // }
    //
    // reset_election_timeout.Stop();
    //
    // if (request.term() >= GetCurrentTerm() && state != State::Follower) {
    //   LOG_DBG("APPEND_ENTRIES: request_term >= current_term; become to FOLLOWER");
    //   BecomeFollower();
    // }
    //
    // if (request.term() > GetCurrentTerm()) {
    //   UpdateTerm(request.term());
    // }
    //
    // VERIFY(request.prev_log_index() != 0 || request.prev_log_term() == 0, "invalid
    // prev_log_term");
    //
    // if (request.prev_log_index() > GetSavedLogSize()) {
    //   LOG_DBG("APPEND_ENTRIES: dismiss: prev_log_index = {} > log_size = {}",
    //           request.prev_log_index(), GetSavedLogSize());
    //   return Ok(std::move(result));
    // }
    //
    // if (request.prev_log_index() != 0) {
    //   const uint64_t log_term = raft_log->Get(request.prev_log_index()).GetValue().term();
    //   if (log_term != request.prev_log_term()) {
    //     LOG_DBG("APPEND_ENTRIES: dismiss: prev_log_term = {} != request_prev_log_term = {}",
    //             log_term, request.prev_log_term());
    //     return Ok(std::move(result));
    //   }
    // }
    //
    // LOG_DBG("APPEND_ENTRIES: accept, writting entries to local log");
    // result.set_success(true);
    //
    // // Check that if leader tries to overwrite the commited log, it must be the same.
    // // The situation when the leader overwrites commited log may occur in case of rpc retries.
    // for (uint64_t index = request.prev_log_index() + 1;
    //      index <= commit_index && index - request.prev_log_index() - 1 < request.entries_size();
    //      ++index) {
    //   using google::protobuf::util::MessageDifferencer;
    //
    //   LogEntry value_from_log = raft_log->Get(index).GetValue();
    //   LogEntry value_from_request = request.entries()[index - request.prev_log_index() - 1];
    //
    //   VERIFY(MessageDifferencer::Equals(value_from_request, value_from_log),
    //          "invalid state: committed log is inconsistent");
    // }
    //
    // auto write_batch = raft_log->MakeWriteBatch();
    // write_batch
    //     .DeleteRange(std::max(request.prev_log_index(), commit_index) + 1,
    //                  std::numeric_limits<uint64_t>::max())
    //     .ExpectOk();
    // for (uint64_t index = std::max(request.prev_log_index(), commit_index) + 1;
    //      index - request.prev_log_index() - 1 < request.entries_size(); ++index) {
    //   write_batch.Put(index, request.entries()[index - request.prev_log_index() - 1]).ExpectOk();
    // }
    // raft_log->Write(std::move(write_batch)).ExpectOk();
    //
    // const uint64_t new_commit_index =
    //     std::max(commit_index, std::min<uint64_t>(request.leader_commit(), GetSavedLogSize()));
    //
    // if (commit_index != new_commit_index) {
    //   LOG_DBG("APPEND_ENTRIES: updating commit index, prev = {}, new = {}", commit_index,
    //           new_commit_index);
    // }
    //
    // for (size_t index = commit_index + 1; index <= new_commit_index; ++index) {
    //   rsm.Apply(raft_log->Get(index).GetValue().request());
    // }
    //
    // commit_index = new_commit_index;
    //
    return Ok(std::move(result));
  }

  Result<RequestVoteRsp, rt::rpc::RpcError> RequestVote(
      const RequestVoteReq& request) noexcept override {
    LOG("REQUEST_VOTE: start, current_term = {}, candidate_id = {}, request_term = {}",
        GetCurrentTerm(), request.candidate_id(), request.term());
    RequestVoteRsp result;
    // result.set_term(GetCurrentTerm());
    // if (request.term() < GetCurrentTerm()) {
    //   LOG_DBG("REQUEST_VOTE: dismiss: request_term < current_term");
    //   result.set_vote_granted(false);
    //   return Ok(std::move(result));
    // }
    //
    // if (request.term() > result.term()) {
    //   LOG_DBG("REQUEST_VOTE: request_term > current_term; change to FOLLOWER");
    //   UpdateTerm(request.term());
    //   if (state != State::Follower) {
    //     BecomeFollower();
    //   }
    // }
    //
    // const auto voted_for = GetVotedFor();
    //
    // if (voted_for == request.candidate_id()) {
    //   LOG_DBG("REQUEST_VOTE: accept: voted_for == candidate_id");
    //   result.set_vote_granted(true);
    //   return Ok(std::move(result));
    // }
    //
    // if (voted_for) {
    //   LOG_DBG("REQUEST_VOTE: dismiss: already voted_for = {}", *voted_for);
    //   result.set_vote_granted(false);
    //   return Ok(std::move(result));
    // }
    //
    // const uint64_t last_log_term = GetLastSavedLogTerm();
    // if (last_log_term > request.last_log_term()) {
    //   LOG_DBG(
    //       "REQUEST_VOTE: dismiss: (current last_log_term = {}) > (candidate last_log_term = {})",
    //       last_log_term, request.last_log_term());
    //   result.set_vote_granted(false);
    // } else if (last_log_term == request.last_log_term()) {
    //   LOG_DBG("REQUEST_VOTE: (current last_log_term = {}) == (candidate last_log_term = {})",
    //           last_log_term, request.last_log_term());
    //   if (GetSavedLogSize() <= request.last_log_index()) {
    //     LOG_DBG("REQUEST_VOTE: accept: (current log_size = {}) <= (candidate log_size = {})",
    //             GetSavedLogSize(), request.last_log_index());
    //     result.set_vote_granted(true);
    //   } else {
    //     LOG_DBG("REQUEST_VOTE: dismiss: (current log_size = {}) > (candidate log_size {})",
    //             GetSavedLogSize(), request.last_log_index());
    //     result.set_vote_granted(false);
    //   }
    // } else {
    //   LOG_DBG("REQUEST_VOTE: accept: (current last_log_term = {}) < (candidate last_log_term =
    //   {})",
    //           last_log_term, request.last_log_term());
    //   result.set_vote_granted(true);
    // }
    // if (result.vote_granted()) {
    //   raft_state->Put("voted_for", request.candidate_id()).ExpectOk();
    // }
    return Ok(std::move(result));
  }

  //////////////////////////////////////////////////////////
  // Node state
  //////////////////////////////////////////////////////////

  void StartNode() noexcept {
    // while (true) {
    //   switch (state) {
    //     case State::Leader:
    //       StartLeader();
    //       break;
    //     case State::Follower:
    //       StartFollower();
    //       break;
    //     case State::Candidate:
    //       StartCandidate();
    //       break;
    //   }
    // }
  }

  //////////////////////////////////////////////////////////

  uint64_t GetCurrentTerm() noexcept {
    return mtf_state->Get("current_term").GetValue();
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

  // Contains two keys: "current_term" and "voted_for"
  StateDbPtr mtf_state;
  // Key is log id
  LogDbPtr mtf_queue;

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
  std::vector<AppendEntriesReq::LogEntry> pending_log;

  //////////////////////////////////////////////////////////
  // Processed client requests
  //////////////////////////////////////////////////////////
};

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

  auto log_db = rt::kv::Open(config.log_db_path, db_options, rt::serde::U64Serde{},
                             rt::serde::ProtobufSerde<AppendEntriesReq::LogEntry>{});
  if (log_db.HasError()) {
    std::string message = fmt::format("cannot open log database at path {}: {}", config.log_db_path,
                                      log_db.GetError().Message());
    LOG_CRIT(message);
    return Err(std::move(message));
  }

  MetamorphosisNode node(config, std::move(state_db.GetValue()), std::move(log_db.GetValue()));

  LOG("Starting raft node, cluster: {}, node_id: {}", ToString(config.raft_nodes), config.node_id);

  rt::rpc::Server server;
  server.Register(static_cast<rt::rpc::MetamorphosisInternalsStub*>(&node));
  server.Register(static_cast<rt::rpc::MetamorphosisApiStub*>(&node));
  server.Start(config.cluster_nodes[config.node_id].port);

  // Metamorphosis node does not support concurrent execution
  boost::fibers::fiber worker([&server]() {
    server.Run();
  });

  node.StartNode();

  server.ShutDown();
  worker.join();

  return Ok();
}

}  // namespace mtf::mtf
