#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <raft/node.h>
#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <cstdlib>

using namespace std::chrono_literals;
using namespace ceq;      // NOLINT
using namespace ceq::rt;  // NOLINT

TEST(RaftElection, SimplyWorks) {
  struct RaftNode final : public sim::IHostRunnable {
    explicit RaftNode(raft::RaftConfig config) : config{std::move(config)} {
    }

    void Main() noexcept override {
      raft::RunMain(config);
    }

    raft::RaftConfig config;
  };

  std::vector<rpc::Endpoint> cluster{
      rpc::Endpoint{"addr1", 42},
      rpc::Endpoint{"addr2", 42},
      rpc::Endpoint{"addr3", 42},
  };
  std::pair<Duration, Duration> election_timeout_interval = {150ms, 300ms};
  rt::Duration heart_beat_period = 50ms;
  rt::Duration rpc_timeout = 50ms;

  RaftNode node1(raft::RaftConfig{
      .node_id = 0,
      .cluster = cluster,
      .election_timeout_interval = election_timeout_interval,
      .heart_beat_period = heart_beat_period,
      .rpc_timeout = rpc_timeout,
  });
  RaftNode node2(raft::RaftConfig{
      .node_id = 1,
      .cluster = cluster,
      .election_timeout_interval = election_timeout_interval,
      .heart_beat_period = heart_beat_period,
      .rpc_timeout = rpc_timeout,
  });
  RaftNode node3(raft::RaftConfig{
      .node_id = 2,
      .cluster = cluster,
      .election_timeout_interval = election_timeout_interval,
      .heart_beat_period = heart_beat_period,
      .rpc_timeout = rpc_timeout,
  });

  sim::WorldOptions world_options{
      .delivery_time_interval = {5ms, 350ms},
  };
  sim::InitWorld(42, world_options);

  sim::AddHost("addr1", &node1);
  sim::AddHost("addr2", &node2);
  sim::AddHost("addr3", &node3);

  sim::RunSimulation(1000);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  _Exit(RUN_ALL_TESTS());
}
