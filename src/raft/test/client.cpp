#include <iostream>

#include <runtime/util/parse/parse.h>

#include <CLI/CLI.hpp>

#include <raft/client/client.h>
#include <raft/test/rsm_msg.pb.h>

using namespace ceq::rt;  // NOLINT

int main(int argc, char** argv) {
  CLI::App app{"raft client"};

  std::vector<Endpoint> raft_nodes;
  app.add_option("--raft-nodes", raft_nodes, "raft nodes endpoints, addr:port")->required();

  Duration global_timeout;
  app.add_option("--timeout", global_timeout, "request global timeout")->default_str("500ms");

  Duration rpc_timeout;
  app.add_option("--timeout", rpc_timeout, "rpc timeout")->default_str("500ms");

  uint64_t attempts;
  app.add_option("--attempts", attempts, "request max attempts")->default_val(10);

  app.set_config("--config", "", "read toml config");
  app.allow_config_extras(false);

  CLI11_PARSE(app, argc, argv);

  ceq::raft::RaftClient client(raft_nodes);

  ceq::raft::RaftClient::Config client_config(global_timeout, rpc_timeout, attempts);

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

  return 0;
}
