#include <runtime/util/parse/parse.h>

#include <CLI/CLI.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace ceq::rt;  // NOLINT

int main(int argc, char** argv) {
  CLI::App app{"Raft history checker"};

  std::vector<Endpoint> raft_nodes;
  app.add_option("--raft-nodes", raft_nodes, "raft nodes endpoints, addr:port")->required();

  Duration global_timeout{};
  app.add_option("--timeout", global_timeout, "request global timeout")->default_val("500ms");

  Duration rpc_timeout{};
  app.add_option("--rps-timeout", rpc_timeout, "rpc timeout")->default_val("500ms");

  uint64_t attempts{};
  app.add_option("--attempts", attempts, "request max attempts")->default_val("10");

  app.set_config("--config", "", "read toml config");
  app.allow_config_extras(false);

  auto interactive =
      app.add_subcommand("interactive", "read commands from command line and perform requests");
  auto random =
      app.add_subcommand("random",
                         "Generate random messages and print debug info to stdout in format:\n"
                         "OK: <invocation_time> <completion_time> <command> [ <cmd0> <cmd1> ... ]\n"
                         "ERR: <error_message>");

  app.require_subcommand(1);

  CLI11_PARSE(app, argc, argv);

  ceq::raft::RaftClient client(raft_nodes);
  ceq::raft::RaftClient::Config client_config(global_timeout, rpc_timeout, attempts);

  if (*interactive) {
    while (std::cin) {
      RsmCommand command;
      std::cout << "> ";
      uint64_t data{};
      std::cin >> data;
      command.set_data(data);
      auto result = client.Apply<RsmResult>(command, client_config);
      if (result.HasError()) {
        std::cout << "> ERROR: " << result.GetError().Message() << std::endl;
      } else {
        std::cout << "> OK: " << result.GetValue() << std::endl;
      }
    }
  } else {
    while (true) {
      RsmCommand command;
      command.set_data(ceq::rt::GetRandomInt());
      auto invocation_time = ceq::rt::Now();
      auto result = client.Apply<RsmResult>(command, client_config);
      auto completion_time = ceq::rt::Now();
      if (result.HasError()) {
        fmt::print("ERROR: {}\n", result.GetError().Message());
      } else {
        fmt::print("OK: {} {} {} [ {} ]\n", invocation_time.time_since_epoch().count(),
                   completion_time.time_since_epoch().count(), command.data(),
                   fmt::join(result.GetValue().log_entries(), " "));
      }
    }
  }

  return 0;
}
