#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <raft/node.h>
#include <runtime/simulator/api.h>

using namespace std::chrono_literals;
using namespace ceq;      // NOLINT
using namespace ceq::rt;  // NOLINT

TEST(RaftElection, SimplyWorks) {
  struct RaftNode final : public ceq::rt::IHostRunnable {
    explicit RaftNode(raft::RaftConfig config) : config{std::move(config)} {
    }

    void Main() noexcept override {
      raft::RunMain(config);
    }

    raft::RaftConfig config;
  };

  std::vector<Endpoint> cluster{
      Endpoint{"addr1", 42},
      Endpoint{"addr2", 42},
      Endpoint{"addr3", 42},
  };
  std::pair<Duration, Duration> election_timeout_interval = {150ms, 300ms};
  rt::Duration heart_beat_period = 50ms;
  rt::Duration rpc_timeout = 50ms;

  RaftNode node1(raft::RaftConfig{
      .node_id = 0,
      .cluster = cluster,
      .election_timeout_interval = election_timeout_interval,
  });
  RaftNode node2(raft::RaftConfig{
      .node_id = 1,
      .cluster = cluster,
      .election_timeout_interval = election_timeout_interval,
  });
  RaftNode node3(raft::RaftConfig{
      .node_id = 2,
      .cluster = cluster,
      .election_timeout_interval = election_timeout_interval,
  });

  WorldOptions world_options{
      .delivery_time_interval = {5ms, 1s},
  };
  ceq::rt::InitWorld(42, world_options);

  ceq::rt::AddHost("addr1", &node1);
  ceq::rt::AddHost("addr2", &node2);
  ceq::rt::AddHost("addr3", &node3);

  ceq::rt::RunSimulation(1000);

  abort();
}
