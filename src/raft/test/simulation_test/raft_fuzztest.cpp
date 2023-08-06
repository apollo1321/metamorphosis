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

struct SleepAction {
  SleepAction() noexcept = default;
  explicit SleepAction(Duration duration) noexcept : duration{duration} {
  }

  Duration duration{};
};

struct HostAction {
  enum class Type {
    Pause,
    Resume,
    Kill,
    Start,
    Restart,
  };

  HostAction() noexcept = default;
  explicit HostAction(Type type) noexcept : type{type} {
  }

  Type type{};
};

struct NetworkAction {
  enum class Type {
    Drop,
    Restore,
  };

  NetworkAction() noexcept = default;
  explicit NetworkAction(Type type) noexcept : type{type} {
  }

  Type type{};
};

using Action = std::variant<SleepAction, HostAction, NetworkAction>;

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

  template <class Container>
  bool Transfer(Container& from, Container& to) {
    if (from.empty()) {
      return false;
    }
    size_t index = rt::GetRandomInt(0, from.size() - 1);
    to.emplace_back(from[index]);
    from.erase(from.begin() + index);
    return true;
  }

  void Main() noexcept override {
    std::vector<rt::Address> killed_hosts;
    std::vector<rt::Address> paused_hosts;
    std::vector<rt::Address> running_hosts = hosts;

    std::vector<std::pair<rt::Address, rt::Address>> alive_connections;
    std::vector<std::pair<rt::Address, rt::Address>> dead_connections;

    for (const auto& from : hosts) {
      for (const auto& to : hosts) {
        alive_connections.emplace_back(from, to);
      }
    }

    for (const auto& action_var : actions) {
      if (std::holds_alternative<SleepAction>(action_var)) {
        auto duration = std::get<SleepAction>(action_var).duration;
        LOG("Sleep for {}", rt::ToString(duration));
        rt::SleepFor(duration);
      } else if (std::holds_alternative<HostAction>(action_var)) {
        const auto& action = std::get<HostAction>(action_var);
        switch (action.type) {
          case HostAction::Type::Pause: {
            if (Transfer(running_hosts, paused_hosts)) {
              LOG("Pause {}", paused_hosts.back());
              rt::sim::PauseHost(paused_hosts.back());
            }
            break;
          }
          case HostAction::Type::Resume: {
            if (Transfer(paused_hosts, running_hosts)) {
              LOG("Resume {}", running_hosts.back());
              rt::sim::ResumeHost(running_hosts.back());
            }
            break;
          }
          case HostAction::Type::Kill: {
            if (Transfer(running_hosts, killed_hosts)) {
              LOG("Kill {}", killed_hosts.back());
              rt::sim::KillHost(killed_hosts.back());
            }
            break;
          }
          case HostAction::Type::Start: {
            if (Transfer(killed_hosts, running_hosts)) {
              LOG("Start {}", running_hosts.back());
              rt::sim::StartHost(running_hosts.back());
            }
            break;
          }
          case HostAction::Type::Restart: {
            if (!running_hosts.empty()) {
              size_t index = rt::GetRandomInt(0, running_hosts.size() - 1);
              rt::sim::KillHost(running_hosts[index]);
              rt::sim::StartHost(running_hosts[index]);
              LOG("Restart {}", running_hosts[index]);
            }
            break;
          }
        }
      } else if (std::holds_alternative<NetworkAction>(action_var)) {
        const auto& action = std::get<NetworkAction>(action_var);
        switch (action.type) {
          case NetworkAction::Type::Drop: {
            if (Transfer(alive_connections, dead_connections)) {
              auto [from, to] = dead_connections.back();
              LOG("Drop {} -> {}", from, to);
              rt::sim::CloseLink(from, to);
            }
            break;
          }
          case NetworkAction::Type::Restore: {
            if (Transfer(dead_connections, alive_connections)) {
              auto [from, to] = alive_connections.back();
              LOG("Restore {} -> {}", from, to);
              rt::sim::RestoreLink(from, to);
            }
            break;
          }
        }
      }
    }
  }

  std::vector<rt::Address> hosts;
  std::vector<Action> actions;
};

static size_t ind = 0;

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
    rt::sim::AddHost(raft_addresses[index], &raft_hosts[index],
                     config.nodes_configs[index].host_config);
    all_addresses.emplace_back(raft_addresses[index]);
  }
  for (size_t index = 0; index < client_hosts.size(); ++index) {
    rt::sim::AddHost(client_addresses[index], &client_hosts[index],
                     config.clients_configs[index].host_config);
    all_addresses.emplace_back(client_addresses[index]);
  }
  Advisory advisory(std::move(all_addresses), actions);
  rt::sim::AddHost("adv", &advisory);

  rt::sim::RunSimulation(config.simulation_duration, 800);
  fmt::print(" {}: Nodes count = {}; Client count = {}; Actions size = {}; History size = {}\n",
             ind++, config.nodes_configs.size(), config.clients_configs.size(), actions.size(),
             history.size());

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
      DurationDomain({0ms, 10ms})                     // max_sleep_lag
  );
}

auto WorldOptionsDomain() noexcept {
  return StructOf<ceq::rt::sim::WorldOptions>(     //
      InRange(0.0, 0.6),                           // network_error_proba
      IntervalDomain({1ms, 100ms}, {1ms, 300ms}),  // delivery_time
      IntervalDomain({1ms, 100ms}, {1ms, 500ms}),  // long_delivery_time
      InRange(0.0, 0.2)                            // long_delivery_time_proba
  );
}

auto RaftConfigDomain() noexcept {
  return StructOf<test::NodeConfig>(               //
      IntervalDomain({1ms, 500ms}, {1ms, 500ms}),  // election_timeout
      DurationDomain({1ms, 500ms}),                // heart_beat_period
      DurationDomain({1ms, 500ms}),                // rpc_timeout
      HostOptionsDomain()                          // host_config
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
      Arbitrary<size_t>(),            // seed
      DurationDomain({20s, 20s}),     // simulation_duration
      WorldOptionsDomain(),           // world_config
      ClientsDomain(),                // clients_configs
      RaftConfigsDomain()             // nodes_configs
  );
}

auto HostActionDomain() noexcept {
  using Type = test::HostAction::Type;
  return ConstructorOf<test::HostAction>(                                                   //
      ElementOf<Type>({Type::Start, Type::Pause, Type::Resume, Type::Kill, Type::Restart})  // type
  );
}

auto NetworkActionDomain() noexcept {
  using Type = test::NetworkAction::Type;
  return ConstructorOf<test::NetworkAction>(        //
      ElementOf<Type>({Type::Drop, Type::Restore})  // type
  );
}

auto ActionDomain() noexcept {
  return VariantOf(                                                    //
      ConstructorOf<test::SleepAction>(DurationDomain({0ms, 100ms})),  //
      HostActionDomain(),                                              //
      NetworkActionDomain()                                            //
  );
}

FUZZ_TEST(RaftFuzzTest, RaftTestWrapper)
    .WithDomains(TestConfigDomain(), VectorOf(ActionDomain()).WithMinSize(2));
