#include <algorithm>
#include <cstdlib>

#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <raft/node.h>
#include <raft/raft.client.h>
#include <raft/raft.pb.h>
#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <raft/client.h>

#include "checker.h"
#include "rsm.h"

using namespace std::chrono_literals;
using namespace ceq;              // NOLINT
using namespace ceq::rt;          // NOLINT
using namespace ceq::raft::test;  // NOLINT

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
  explicit Client(const raft::Cluster& cluster, std::vector<RequestInfo>& history) noexcept
      : cluster{cluster}, history{history} {
  }

  void Main() noexcept override {
    raft::RaftClient client(cluster);

    while (true) {
      RsmCommand command;
      command.set_data(std::uniform_int_distribution<uint8_t>()(GetGenerator()));

      LOG("CLIENT: write command: {}", command.data());

      RequestInfo info;
      info.command = command.data();
      info.start = sim::GetGlobalTime();
      auto response = FromAny<RsmResult>(client.Execute(ToAny(command), 100s, 1000).GetValue());
      LOG("CLIENT: response = {}", response);

      info.end = sim::GetGlobalTime();
      info.result.resize(response.log_entries_size());
      std::copy(response.log_entries().begin(), response.log_entries().end(), info.result.begin());
      history.emplace_back(std::move(info));
    }
  }

  raft::Cluster cluster;
  std::vector<RequestInfo>& history;
};

std::vector<RaftNode> MakeRaftNodes(const raft::RaftConfig& base_config) noexcept {
  std::vector<RaftNode> nodes;
  for (size_t index = 0; index < base_config.cluster.size(); ++index) {
    raft::RaftConfig config = base_config;
    config.node_id = index;
    nodes.emplace_back(config);
  }
  return nodes;
}

std::vector<Client> MakeClients(size_t count, const raft::Cluster& raft_cluster,
                                std::vector<RequestInfo>& history) noexcept {
  std::vector<Client> nodes;
  for (size_t index = 0; index < count; ++index) {
    nodes.emplace_back(raft_cluster, history);
  }
  return nodes;
}

void SpawnRaftNodes(std::vector<RaftNode>& nodes, const sim::HostOptions& host_options) noexcept {
  for (size_t index = 0; index < nodes.size(); ++index) {
    sim::AddHost(nodes[index].config.cluster[index].address, &nodes[index], host_options);
  }
}

void SpawnClients(std::vector<Client>& nodes, const sim::HostOptions& host_options) noexcept {
  for (size_t index = 0; index < nodes.size(); ++index) {
    sim::AddHost("client" + std::to_string(index), &nodes[index], host_options);
  }
}

std::vector<rpc::Endpoint> GenerateRaftCluster(size_t size) noexcept {
  std::vector<rpc::Endpoint> result;
  for (size_t index = 0; index < size; ++index) {
    result.emplace_back(rpc::Endpoint{"addr" + std::to_string(index), 42});
  }
  return result;
}

void RunTest(size_t raft_nodes_count, size_t clients_count) noexcept {
  auto raft_cluster = GenerateRaftCluster(raft_nodes_count);

  raft::RaftConfig config{
      .cluster = raft_cluster,
      .election_timeout_interval = {150ms, 300ms},
      .heart_beat_period = 50ms,
      .rpc_timeout = 90ms,
  };

  sim::WorldOptions world_options{.delivery_time_interval = {0ms, 100ms},
                                  .network_error_proba = 0.2};

  sim::HostOptions raft_host_options{.max_sleep_lag = 2ms};

  auto nodes = MakeRaftNodes(config);

  for (size_t iteration = 0; iteration < 50; ++iteration) {
    std::vector<RequestInfo> history;

    auto clients = MakeClients(clients_count, raft_cluster, history);

    sim::InitWorld(iteration, world_options);
    SpawnClients(clients, sim::HostOptions{});
    SpawnRaftNodes(nodes, raft_host_options);
    sim::RunSimulation(15s);

    CheckLinearizability(std::move(history));
  }
}

TEST(Raft, ThreeReplicasOneClient) {
  RunTest(3, 1);
}

TEST(Raft, ThreeReplicasTwoClients) {
  RunTest(3, 2);
}

TEST(Raft, FiveReplicasThreeClients) {
  RunTest(5, 3);
}

TEST(Raft, SixReplicasTenClients) {
  RunTest(6, 10);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  _Exit(RUN_ALL_TESTS());
}
