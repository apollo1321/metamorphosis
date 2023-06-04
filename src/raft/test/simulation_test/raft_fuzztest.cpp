#include "hosts.h"

#include <cstdlib>
#include <variant>

#include <fuzztest/fuzztest.h>
#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <raft/test/util/history_checker.h>

#include "runtime/util/print/print.h"

using namespace std::chrono_literals;

namespace ceq::raft::test {

using rt::Duration;

struct Sleep {
  Duration duration{};
};

struct HostAction {
  enum class Type {
    Pause,
    Resume,
    Kill,
    Start,
  };

  size_t host_id{};
  Type type{};
};

struct NetworkAction {
  enum class Type {
    Drop,
    Restore,
  };

  size_t from{};
  size_t to{};

  Type type{};
};

using Action = std::variant<Sleep, HostAction, NetworkAction>;

struct ClientConfig {
  raft::RaftClient::Config raft_config;
  rt::sim::HostOptions host_config;
};

struct NodeConfig {
  rt::Interval election_timeout;
  rt::Duration heart_beat_period;
  rt::Duration rpc_timeout;

  rt::sim::HostOptions host_config{};
};

struct TestConfig {
  size_t seed;
  Duration simulation_duration;

  rt::sim::WorldOptions world_config;
  std::vector<ClientConfig> clients_configs;
  std::vector<NodeConfig> nodes_configs;
};

struct Advisory final : public rt::sim::IHostRunnable {
  explicit Advisory(std::vector<rt::Address> hosts, std::vector<Action> actions) noexcept
      : hosts{hosts}, actions{actions} {
  }

  void Main() noexcept override {
    for (const auto& action_var : actions) {
      if (std::holds_alternative<Sleep>(action_var)) {
        rt::SleepFor(std::get<Sleep>(action_var).duration);
      } else if (std::holds_alternative<HostAction>(action_var)) {
        const auto& action = std::get<HostAction>(action_var);
        const auto& address = hosts[action.host_id];
        switch (action.type) {
          case HostAction::Type::Pause:
            rt::sim::PauseHost(address);
            break;
          case HostAction::Type::Resume:
            rt::sim::ResumeHost(address);
            break;
          case HostAction::Type::Kill:
            rt::sim::KillHost(address);
            break;
          case HostAction::Type::Start:
            rt::sim::StartHost(address);
            break;
        }
      } else if (std::holds_alternative<NetworkAction>(action_var)) {
        const auto& action = std::get<NetworkAction>(action_var);
        const auto& from = hosts[action.from];
        const auto& to = hosts[action.to];

        switch (action.type) {
          case NetworkAction::Type::Drop:
            rt::sim::CloseLink(from, to);
            break;
          case NetworkAction::Type::Restore:
            rt::sim::RestoreLink(from, to);
            break;
        }
      }
    }
  }

  std::vector<rt::Address> hosts;
  std::vector<Action> actions;
};

size_t ind = 0;

void RunRaftTest(TestConfig config, std::vector<Action> actions) noexcept {
  std::vector<ceq::rt::Address> raft_addresses;
  std::vector<ceq::rt::Endpoint> raft_endpoints;
  for (size_t index = 0; index < config.nodes_configs.size(); ++index) {
    raft_addresses.emplace_back("node_" + std::to_string(index));
    raft_endpoints.emplace_back(raft_addresses.back(), 42);
  }
  std::vector<ceq::rt::Address> client_addresses;
  for (size_t index = 0; index < config.clients_configs.size(); ++index) {
    client_addresses.emplace_back("client_" + std::to_string(index));
  }

  std::vector<ceq::raft::test::RaftHost> raft_hosts;
  for (size_t index = 0; index < config.nodes_configs.size(); ++index) {
    const auto& node_config = config.nodes_configs[index];
    RaftConfig raft_config;
    raft_config.rpc_timeout = node_config.rpc_timeout;
    raft_config.election_timeout = node_config.election_timeout;
    raft_config.heart_beat_period = node_config.heart_beat_period;

    raft_config.node_id = index;
    raft_config.raft_nodes = raft_endpoints;
    raft_config.log_db_path = "/tmp/raft_log";
    raft_config.raft_state_db_path = "/tmp/raft_state";

    raft_hosts.emplace_back(raft_config);
  }

  std::vector<RequestInfo> history;

  std::vector<ceq::raft::test::RaftClientHost> client_hosts;
  for (size_t client_id = 0; client_id < config.clients_configs.size(); ++client_id) {
    client_hosts.emplace_back(raft_endpoints, history,
                              config.clients_configs[client_id].raft_config);
  }

  rt::sim::InitWorld(config.seed, config.world_config);

  std::vector<rt::Address> all_addresses;
  for (size_t index = 0; index < config.nodes_configs.size(); ++index) {
    rt::sim::AddHost(raft_addresses[index], &raft_hosts[index]);
    all_addresses.emplace_back(raft_addresses[index]);
  }
  for (size_t index = 0; index < client_hosts.size(); ++index) {
    rt::sim::AddHost(client_addresses[index], &client_hosts[index]);
    all_addresses.emplace_back(client_addresses[index]);
  }
  Advisory advisory(std::move(all_addresses), actions);
  rt::sim::AddHost("adv", &advisory);

  rt::sim::RunSimulation(config.simulation_duration, 400);

  std::cerr << "[" << ind++ << "]: "                                 //
            << "Nodes count = " << config.nodes_configs.size()       //
            << "; Client count = " << config.clients_configs.size()  //
            << "; Actions size = " << actions.size()                 //
            << "; History size = " << history.size()                 //
            << std::endl;

  auto check_result = CheckLinearizability(std::move(history));
  if (check_result.HasError()) {
    FAIL() << "linearizability check failed, seed = " << config.seed << ": "
           << check_result.GetError();
  }
}

}  // namespace ceq::raft::test

using namespace ceq::raft;  // NOLINT
using namespace fuzztest;   // NOLINT

void RaftTestWrapper(test::TestConfig config, std::vector<test::Action> actions) {
  const size_t total_hosts = config.nodes_configs.size() + config.clients_configs.size();
  for (auto& action_var : actions) {
    if (std::holds_alternative<test::HostAction>(action_var)) {
      auto& action = std::get<test::HostAction>(action_var);
      action.host_id = action.host_id % total_hosts;
    } else if (std::holds_alternative<test::NetworkAction>(action_var)) {
      auto& action = std::get<test::NetworkAction>(action_var);
      action.from = action.from % total_hosts;
      action.to = action.to % total_hosts;
    }
  }
  test::RunRaftTest(config, actions);
}

auto DurationDomain(ceq::rt::Interval interval) noexcept {
  return ConstructorOf<ceq::rt::Duration>(InRange(interval.from.count(), interval.to.count()));
}

auto IntervalDomain(ceq::rt::Interval low_interval, ceq::rt::Interval high_interval) noexcept {
  return Filter(
      [](ceq::rt::Interval interval) {
        return interval.from <= interval.to;
      },
      StructOf<ceq::rt::Interval>(DurationDomain(low_interval), DurationDomain(high_interval)));
}

auto HostOptionsDomain() noexcept {
  return StructOf<ceq::rt::sim::HostOptions>(         //
      IntervalDomain({0ms, 100ms}, {0ms, 100ms}),     // start_time
      PairOf(InRange(0.0, 0.05), InRange(0., 0.05)),  // drift_interval
      DurationDomain({0ms, 5ms})                      // max_sleep_lag
  );
}

auto WorldOptionsDomain() noexcept {
  return StructOf<ceq::rt::sim::WorldOptions>(     //
      InRange(0.0, 0.6),                           // network_error_proba
      IntervalDomain({1ms, 100ms}, {1ms, 300ms}),  // delivery_time
      IntervalDomain({1ms, 100ms}, {1ms, 500ms}),  // long_delivery_time
      InRange(0.0, 0.1)                            // long_delivery_time_proba
  );
}

auto RaftConfigDomain() noexcept {
  return StructOf<test::NodeConfig>(                  //
      IntervalDomain({50ms, 500ms}, {100ms, 500ms}),  // election_timeout
      DurationDomain({50ms, 500ms}),                  // heart_beat_period
      DurationDomain({500ms, 500ms}),                 // rpc_timeout
      HostOptionsDomain()                             // host_config
  );
}

auto RaftClientDomain() noexcept {
  return ConstructorOf<RaftClient::Config>(  //
      DurationDomain({800ms, 1000ms}),       // global_timeout
      DurationDomain({200ms, 400ms}),        // rpc_timeout
      InRange(1, 10)                         // retry_count
  );
}

auto ClientDomain() noexcept {
  return StructOf<test::ClientConfig>(RaftClientDomain(), HostOptionsDomain());
}

auto ClientsDomain() noexcept {
  return VectorOf(ClientDomain()).WithMinSize(1).WithMaxSize(8);
}

auto RaftConfigsDomain() noexcept {
  return VectorOf(RaftConfigDomain()).WithMinSize(3).WithMaxSize(8);
}

auto TestConfigDomain() noexcept {
  return StructOf<test::TestConfig>(  //
      ElementOf({42}),                // seed
      DurationDomain({10s, 10s}),     // simulation_duration
      WorldOptionsDomain(),           // world_config
      ClientsDomain(),                // clients_configs
      RaftConfigsDomain()             // nodes_configs
  );
}

auto HostActionDomain() noexcept {
  using Type = test::HostAction::Type;
  return StructOf<test::HostAction>(                                         //
      Arbitrary<size_t>(),                                                   // host_id
      ElementOf<Type>({Type::Start, Type::Pause, Type::Resume, Type::Kill})  // type
  );
}

auto NetworkActionDomain() noexcept {
  using Type = test::NetworkAction::Type;
  return StructOf<test::NetworkAction>(             //
      Arbitrary<size_t>(),                          // from
      Arbitrary<size_t>(),                          // to
      ElementOf<Type>({Type::Drop, Type::Restore})  // type
  );
}

auto ActionDomain() noexcept {
  return VariantOf(                                         //
      StructOf<test::Sleep>(DurationDomain({0ms, 100ms})),  //
      HostActionDomain(),                                   //
      NetworkActionDomain()                                 //
  );
}

FUZZ_TEST(RaftFuzzTest, RaftTestWrapper)
    .WithDomains(TestConfigDomain(), VectorOf(ActionDomain()).WithMaxSize(20));
