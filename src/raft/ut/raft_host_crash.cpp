#include <cstdlib>

#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include "history_checker.h"
#include "hosts.h"

using namespace std::chrono_literals;

namespace ceq::raft::test {

void RunTestWithPauses(size_t seed, size_t raft_nodes_count, size_t clients_count,
                       size_t max_crashed_host_count) noexcept {
  std::vector<rt::rpc::Endpoint> raft_nodes;
  for (size_t index = 0; index < raft_nodes_count; ++index) {
    raft_nodes.emplace_back(rt::rpc::Endpoint{"addr" + std::to_string(index), 42});
  }

  std::vector<RaftHost> raft_hosts;
  for (size_t node_id = 0; node_id < raft_nodes.size(); ++node_id) {
    raft_hosts.emplace_back(raft::RaftConfig{.node_id = node_id,
                                             .raft_nodes = raft_nodes,
                                             .election_timeout_interval = {150ms, 300ms},
                                             .heart_beat_period = 50ms,
                                             .rpc_timeout = 90ms,
                                             .log_db_path = "/tmp/raft_log",
                                             .raft_state_db_path = "/tmp/raft_state"});
  }

  std::vector<RequestInfo> history;

  RaftClientHost client_host(raft_nodes, history, 1s, 10);

  CrashSupervisor supervisor(raft_nodes, 2s);

  rt::sim::InitWorld(seed, rt::sim::WorldOptions{.delivery_time_interval = {0ms, 100ms},
                                                 .network_error_proba = 0.1});
  for (auto& host : raft_hosts) {
    rt::sim::AddHost(host.config.raft_nodes[host.config.node_id].address, &host,
                     rt::sim::HostOptions{.max_sleep_lag = 1ms});
  }
  for (size_t index = 0; index < clients_count; ++index) {
    rt::sim::AddHost("client" + std::to_string(index), &client_host);
  }
  for (size_t index = 0; index < max_crashed_host_count; ++index) {
    rt::sim::AddHost("supervisor" + std::to_string(index), &supervisor);
  }

  rt::sim::RunSimulation(15s);

  EXPECT_GT(history.size(), 1u) << "Too few requests have been completed, seed = " << seed;
  if (testing::Test::HasNonfatalFailure()) {
    return;
  }

  if (!CheckLinearizability(std::move(history))) {
    FAIL() << "linearizability check failed, seed = " << seed;
    return;
  }
}

}  // namespace ceq::raft::test

TEST(RaftHostPause, Replica3Client1Crashed1) {
  for (size_t seed = 0; seed < 50; ++seed) {
    ceq::raft::test::RunTestWithPauses(seed, 3, 1, 1);
    if (testing::Test::HasNonfatalFailure()) {
      return;
    }
  }
}

TEST(RaftHostPause, Replica3Client3Crashed1) {
  for (size_t seed = 0; seed < 50; ++seed) {
    ceq::raft::test::RunTestWithPauses(seed + 100, 3, 3, 1);
    if (testing::Test::HasNonfatalFailure()) {
      return;
    }
  }
}

TEST(RaftHostPause, Replica5Client3Crashed1) {
  for (size_t seed = 0; seed < 50; ++seed) {
    ceq::raft::test::RunTestWithPauses(seed + 200, 5, 3, 1);
    if (testing::Test::HasNonfatalFailure()) {
      return;
    }
  }
}

TEST(RaftHostPause, Replica5Client2Crashed2) {
  for (size_t seed = 0; seed < 50; ++seed) {
    ceq::raft::test::RunTestWithPauses(seed + 300, 5, 2, 2);
    if (testing::Test::HasNonfatalFailure()) {
      return;
    }
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  _Exit(RUN_ALL_TESTS());
}
