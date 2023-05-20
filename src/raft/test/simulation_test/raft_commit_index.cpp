#include "hosts.h"

#include <cstdlib>

#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <raft/test/util/history_checker.h>

using namespace std::chrono_literals;
using namespace ceq;              // NOLINT
using namespace ceq::raft::test;  // NOLINT

/*
 * This test is intended to reproduce execution of raft algorithm as was shown in raft paper,
 * fig. 8.
 *
 * Test states:
 *
 *    |   (a)  |   (b)  |   (c)  |   (d)  |
 * ---+--------+--------+--------+--------+
 * S0 | * 1 2  |   1 2  | * 1 2  |   1 3  |
 * S1 |   1 2  |   1 2  |   1 2  |   1 3  |
 * S2 |   1    |   1    |   1 2  |   1 3  |
 * S3 |   1    |   1    |   1    |   1 3  |
 * S4 |   1    | * 1 3  |   1 3  | * 1 3  |
 * ---+--------+--------+--------+--------+
 *
 */

namespace ceq::raft::test {

struct Supervisor final : public rt::sim::IHostRunnable {
  explicit Supervisor(const std::vector<rt::Endpoint>& raft_nodes,
                      std::vector<RequestInfo>& history, rt::Duration timeout,
                      size_t retry_count) noexcept
      : raft_nodes{raft_nodes}, history{history}, timeout{timeout}, retry_count{retry_count} {
  }

  void Main() noexcept override {
    // Pause node S4 for node S0 to become leader
    rt::sim::PauseHost(raft_nodes[4].address);
    rt::SleepFor(300ms);
    // S0 is elected now.
    // Resume node S4
    rt::sim::ResumeHost(raft_nodes[4].address);

    // Replicate 1 to all replicas
    raft::RaftClient client0({raft_nodes[0]});
    RsmCommand cmd;
    cmd.set_data(1);
    auto response = client0.Apply<RsmResult>(cmd, RaftClient::Config{1h, 1s, 10});
    EXPECT_EQ(response.GetValue().log_entries()[0], 1);

    rt::SleepFor(100ms);

    //////////////////////////////////////////////////////////
    // Reaching state (a)
    //////////////////////////////////////////////////////////
    LOG("REACHING STATE (a)");

    // Drop links between S0 and {S2, S3, S4}, and pause these
    rt::sim::CloseLinkBidirectional(raft_nodes[0].address, raft_nodes[2].address);
    rt::sim::CloseLinkBidirectional(raft_nodes[0].address, raft_nodes[3].address);
    rt::sim::CloseLinkBidirectional(raft_nodes[0].address, raft_nodes[4].address);

    rt::sim::PauseHost(raft_nodes[2].address);
    rt::sim::PauseHost(raft_nodes[3].address);
    rt::sim::PauseHost(raft_nodes[4].address);

    cmd.set_data(2);
    client0.Apply<RsmResult>(cmd, RaftClient::Config{100ms, 100ms, 1}).ExpectFail();

    rt::SleepFor(100ms);

    //////////////////////////////////////////////////////////
    // Reaching state (b)
    //////////////////////////////////////////////////////////
    LOG("REACHING STATE (b)");

    // Restart node S0
    rt::sim::KillHost(raft_nodes[0].address);
    rt::sim::StartHost(raft_nodes[0].address);

    // Pause node S0, S1
    rt::sim::PauseHost(raft_nodes[0].address);
    rt::sim::PauseHost(raft_nodes[1].address);

    // Resume all other nodes
    rt::sim::ResumeHost(raft_nodes[2].address);
    rt::sim::ResumeHost(raft_nodes[3].address);
    rt::sim::ResumeHost(raft_nodes[4].address);

    // Restore all links
    RestoreAllLinks();

    // Close link between S4 and {S0, S1} for S4 not to overwrite their logs
    rt::sim::CloseLinkBidirectional(raft_nodes[4].address, raft_nodes[0].address);
    rt::sim::CloseLinkBidirectional(raft_nodes[4].address, raft_nodes[1].address);

    // Wait for S4 to become leader
    rt::SleepFor(300ms);

    // S4 should be a leader, now drop links from S4 to all other replicas
    rt::sim::CloseLinkBidirectional(raft_nodes[4].address, raft_nodes[2].address);
    rt::sim::CloseLinkBidirectional(raft_nodes[4].address, raft_nodes[3].address);

    // Send request to S4
    raft::RaftClient client4({raft_nodes[4]});
    cmd.set_data(3);
    client4.Apply<RsmResult>(cmd, RaftClient::Config(100ms, 100ms, 1)).ExpectFail();

    //////////////////////////////////////////////////////////
    // Reaching state (c)
    //////////////////////////////////////////////////////////
    LOG("REACHING STATE (c)");

    // Restart S4
    rt::sim::KillHost(raft_nodes[4].address);
    rt::sim::StartHost(raft_nodes[4].address);

    // Pause S4
    rt::sim::PauseHost(raft_nodes[4].address);

    // Resume S0, S1
    rt::sim::ResumeHost(raft_nodes[0].address);
    rt::sim::ResumeHost(raft_nodes[1].address);

    // Wait for S1 to become leader and to replicate 2 to S3
    rt::SleepFor(1s);

    //////////////////////////////////////////////////////////
    // Reaching state (d)
    //////////////////////////////////////////////////////////
    LOG("REACHING STATE (d)");

    RestoreAllLinks();

    // Restart S0
    rt::sim::KillHost(raft_nodes[0].address);
    rt::sim::StartHost(raft_nodes[0].address);

    // Pause S0
    rt::sim::PauseHost(raft_nodes[0].address);

    // Resume S4
    rt::sim::ResumeHost(raft_nodes[4].address);

    // Wait for S4 to become leader
    rt::SleepFor(300ms);

    // Finally resume S0
    rt::sim::ResumeHost(raft_nodes[0].address);

    // Now S4 should replicate its log to all other replicas
    rt::SleepFor(500ms);

    //////////////////////////////////////////////////////////
    // Checking log consistency
    //////////////////////////////////////////////////////////
    LOG("CHECKING CONSISTENCY");

    history.emplace_back(SendCommand(client4, 4));
    rt::SleepFor(300ms);

    // Pause S4 for S0 to become leader
    rt::sim::PauseHost(raft_nodes[4].address);
    rt::SleepFor(1s);

    history.emplace_back(SendCommand(client0, 5));

    // Pause S0 for one of {S1, S2, S3} to become leader
    rt::sim::PauseHost(raft_nodes[0].address);

    rt::SleepFor(4s);
    RaftClient client(raft_nodes);
    history.emplace_back(SendCommand(client, 6));

    LOG("FINISHED");

    finished = true;
  }

  raft::test::RequestInfo SendCommand(RaftClient& client, uint64_t data) noexcept {
    raft::test::RequestInfo info;
    info.command = data;
    RsmCommand cmd;
    cmd.set_data(info.command);
    info.invocation_time = rt::Now();
    auto response = client.Apply<RsmResult>(cmd, RaftClient::Config(10s, 300ms, 100)).GetValue();
    info.completion_time = rt::Now();
    info.result =
        std::vector<uint64_t>(response.log_entries().begin(), response.log_entries().end());
    return info;
  }

  void RestoreAllLinks() {
    for (const auto& first : raft_nodes) {
      for (const auto& second : raft_nodes) {
        rt::sim::RestoreLinkBidirectional(first.address, second.address);
      }
    }
  }

  std::vector<rt::Endpoint> raft_nodes;
  std::vector<RequestInfo>& history;
  rt::Duration timeout;
  size_t retry_count;

  bool finished = false;
};

}  // namespace ceq::raft::test

TEST(RaftCommit, Replica5Client1) {
  constexpr size_t kSeed = 42;

  std::vector<rt::Endpoint> raft_nodes;
  for (size_t index = 0; index < 5; ++index) {
    raft_nodes.emplace_back(rt::Endpoint{"addr" + std::to_string(index), 42});
  }

  std::vector<RaftHost> raft_hosts;

  for (size_t node_id = 0; node_id < raft_nodes.size(); ++node_id) {
    rt::Interval election_timeout;
    if (node_id == 0 || node_id == 4) {
      election_timeout = {150ms, 150ms};
    } else {
      election_timeout = {900ms, 1500ms};
    }

    raft_hosts.emplace_back(raft::RaftConfig{.node_id = node_id,
                                             .raft_nodes = raft_nodes,
                                             .election_timeout = election_timeout,
                                             .heart_beat_period = 50ms,
                                             .rpc_timeout = 90ms,
                                             .log_db_path = "/tmp/raft_log",
                                             .raft_state_db_path = "/tmp/raft_state"});
  }

  std::vector<RequestInfo> history;

  Supervisor supervisor(raft_nodes, history, 1s, 10);

  rt::sim::InitWorld(kSeed, rt::sim::WorldOptions{
                                .network_error_proba = 0.0,
                                .delivery_time = {30ms, 30ms},
                            });

  for (auto& host : raft_hosts) {
    rt::sim::AddHost(host.config.raft_nodes[host.config.node_id].address, &host,
                     rt::sim::HostOptions{.max_sleep_lag = 1ms});
  }
  rt::sim::AddHost("supervisor", &supervisor);

  rt::sim::RunSimulation(50s);

  EXPECT_TRUE(supervisor.finished);

  auto check_result = CheckLinearizability(std::move(history));
  if (check_result.HasError()) {
    FAIL() << "linearizability check failed, seed = " << kSeed << ": " << check_result.GetError();
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  _Exit(RUN_ALL_TESTS());
}
