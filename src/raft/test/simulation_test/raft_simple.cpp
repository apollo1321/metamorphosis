#include "hosts.h"

#include <cstdlib>

#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <raft/test/util/history_checker.h>

using namespace std::chrono_literals;

namespace ceq::raft::test {

void RunSimpleTest(size_t seed, size_t raft_nodes_count, size_t clients_count) noexcept {
  std::vector<rt::Endpoint> raft_nodes;
  for (size_t index = 0; index < raft_nodes_count; ++index) {
    raft_nodes.emplace_back(rt::Endpoint{"addr" + std::to_string(index), 42});
  }

  std::vector<RaftHost> raft_hosts;
  for (size_t node_id = 0; node_id < raft_nodes.size(); ++node_id) {
    raft_hosts.emplace_back(raft::RaftConfig{.node_id = node_id,
                                             .raft_nodes = raft_nodes,
                                             .election_timeout = {150ms, 300ms},
                                             .heart_beat_period = 50ms,
                                             .rpc_timeout = 90ms,
                                             .log_db_path = "/tmp/raft_log",
                                             .raft_state_db_path = "/tmp/raft_state"});
  }

  std::vector<RequestInfo> history;

  RaftClientHost client_host(raft_nodes, history, RaftClient::Config{1s, 300ms, 10});

  rt::sim::InitWorld(seed, rt::sim::WorldOptions{
                               .network_error_proba = 0.1,
                               .delivery_time = {0ms, 50ms},
                               .long_delivery_time = {100ms, 500ms},
                               .long_delivery_time_proba = 0.1,
                           });

  for (auto& host : raft_hosts) {
    rt::sim::AddHost(host.config.raft_nodes[host.config.node_id].address, &host,
                     rt::sim::HostOptions{.max_sleep_lag = 1ms});
  }
  for (size_t index = 0; index < clients_count; ++index) {
    rt::sim::AddHost("client" + std::to_string(index), &client_host);
  }

  rt::sim::RunSimulation(10s);

  EXPECT_GT(history.size(), 1u) << "Too few requests have been completed, seed = " << seed;
  if (testing::Test::HasNonfatalFailure()) {
    return;
  }

  auto check_result = CheckLinearizability(std::move(history));
  if (check_result.HasError()) {
    FAIL() << "linearizability check failed, seed = " << seed << ": " << check_result.GetError();
  }
  
}

}  // namespace ceq::raft::test

TEST(RaftSimple, Replica3Client1) {
  for (size_t seed = 0; seed < 30; ++seed) {
    ceq::raft::test::RunSimpleTest(seed, 3, 1);
    if (testing::Test::HasNonfatalFailure()) {
      return;
    }
  }
}

TEST(RaftSimple, Replica3Client2) {
  for (size_t seed = 0; seed < 30; ++seed) {
    ceq::raft::test::RunSimpleTest(seed + 100, 3, 2);
    if (testing::Test::HasNonfatalFailure()) {
      return;
    }
  }
}

TEST(RaftSimple, Replica5Client3) {
  for (size_t seed = 0; seed < 30; ++seed) {
    ceq::raft::test::RunSimpleTest(seed + 200, 5, 3);
    if (testing::Test::HasNonfatalFailure()) {
      return;
    }
  }
}

TEST(RaftSimple, Replica6Client10) {
  for (size_t seed = 0; seed < 30; ++seed) {
    ceq::raft::test::RunSimpleTest(seed + 300, 6, 10);
    if (testing::Test::HasNonfatalFailure()) {
      return;
    }
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  _Exit(RUN_ALL_TESTS());
}
