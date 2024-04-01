#include <raft/client/client.h>
#include <raft/test/util/logging_state_machine.pb.h>

#include <runtime/util/parse/parse.h>

#include <CLI/CLI.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace mtf::rt;  // NOLINT

int main(int argc, char** argv) {
  CLI::App app{"Raft client"};

  std::vector<Endpoint> raft_nodes;
  app.add_option("--raft-nodes", raft_nodes, "Raft nodes endpoints, addr:port")->required();

  Duration global_timeout{};
  app.add_option("--timeout", global_timeout, "Request global timeout")->default_val("500ms");

  Duration rpc_timeout{};
  app.add_option("--rpc-timeout", rpc_timeout, "Rpc timeout")->default_val("500ms");

  uint64_t attempts{};
  app.add_option("--attempts", attempts, "Request max attempts")->default_val("10");

  BackoffParams backoff_params;
  app.add_option("--backoff-initial", backoff_params.initial, "Initial backoff")
      ->default_val("5ms");
  app.add_option("--backoff-max", backoff_params.max, "Max backoff")->default_val("1s");
  app.add_option("--backoff-factor", backoff_params.factor, "Backoff factor")->default_val("2.0");

  app.set_config("--config", "", "Read toml config");
  app.allow_config_extras(false);

  auto interactive =
      app.add_subcommand("interactive", "read commands from command line and perform requests");
  auto random = app.add_subcommand(
      "random",
      "Generate random messages and print debug info to stdout in format:\n"
      "OK: <invocation_time> <completion_time> <command> [ <cnt> <cmd0> <cmd1> ... ]\n"
      "ERR: <error_message>");

  app.require_subcommand(1);

  CLI11_PARSE(app, argc, argv);

  mtf::raft::RaftClient client(raft_nodes);
  mtf::raft::RaftClient::Config client_config(global_timeout, rpc_timeout, attempts,
                                              backoff_params);

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
      command.set_data(mtf::rt::GetRandomInt());
      auto invocation_time = mtf::rt::Now();
      auto result = client.Apply<RsmResult>(command, client_config);
      auto completion_time = mtf::rt::Now();
      if (result.HasError()) {
        fmt::print("ERROR: {}\n", result.GetError().Message());
      } else {
        fmt::print("OK: {} {} {} [ {} {} ]\n", invocation_time.time_since_epoch().count(),
                   completion_time.time_since_epoch().count(), command.data(),
                   result.GetValue().log_entries_size(),
                   fmt::join(result.GetValue().log_entries(), " "));
      }
      std::fflush(nullptr);
    }
  }

  return 0;
}
