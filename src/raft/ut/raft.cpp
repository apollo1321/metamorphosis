#include <cstdlib>

#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <raft/node.h>
#include <raft/raft.client.h>
#include <raft/raft.pb.h>
#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <raft/client.h>

#include "test_rsm.h"

using namespace std::chrono_literals;
using namespace ceq;      // NOLINT
using namespace ceq::rt;  // NOLINT

TEST(RaftElection, SimplyWorks) {
  struct RaftNode final : public sim::IHostRunnable {
    explicit RaftNode(raft::RaftConfig config) noexcept : config{std::move(config)} {
    }

    void Main() noexcept override {
      TestStateMachine state_machine;
      raft::RunMain(&state_machine, config);
    }

    raft::RaftConfig config;
  };

  struct Client final : public sim::IHostRunnable {
    explicit Client(const std::vector<rpc::Endpoint>& cluster) noexcept : client{cluster} {
    }

    void Main() noexcept override {
      SleepFor(500ms);

      for (size_t index = 0;; ++index) {
        LOG("CLIENT: write command: {}", index);

        RsmCommand command;
        command.set_data(index);
        auto response = FromAny<RsmResponse>(client.Execute(ToAny(command), 100s, 100).GetValue());

        EXPECT_EQ(response.log_entries_size(), index + 1);
        LOG("CLIENT: response = {}", response);
        for (size_t entry_index = 0; entry_index < response.log_entries_size(); ++entry_index) {
          EXPECT_EQ(response.log_entries()[entry_index], entry_index);
        }
        LOG("CLIENT: finished writing command: {}", index);
      }
    }

    raft::RaftClient client;
  };

  std::vector<rpc::Endpoint> cluster{
      rpc::Endpoint{"addr0", 42},
      rpc::Endpoint{"addr1", 42},
      rpc::Endpoint{"addr2", 42},
  };
  std::pair<Duration, Duration> election_timeout_interval = {150ms, 300ms};
  rt::Duration heart_beat_period = 50ms;
  rt::Duration rpc_timeout = 90ms;

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

  Client client{cluster};

  for (size_t iteration = 0; iteration < 20; ++iteration) {
    // One-way delay
    sim::WorldOptions world_options{.delivery_time_interval = {5ms, 50ms},
                                    .network_error_proba = 0.2};
    sim::InitWorld(iteration, world_options);

    sim::AddHost("client", &client);
    sim::AddHost("addr0", &node1);
    sim::AddHost("addr1", &node2);
    sim::AddHost("addr2", &node3);

    sim::RunSimulation(10s);
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  _Exit(RUN_ALL_TESTS());
}
