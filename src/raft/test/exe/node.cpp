#include <raft/test/util/logging_state_machine.h>
#include <raft/test/util/logging_state_machine.pb.h>

#include <raft/node/node.h>

#include <runtime/util/parse/parse.h>

#include <CLI/CLI.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace ceq::rt;  // NOLINT

int main(int argc, char** argv) {
  CLI::App app{"Raft node"};

  ceq::raft::RaftConfig config;

  // Nodes
  app.add_option("--raft-nodes", config.raft_nodes, "Raft nodes endpoints, addr:port")->required();
  app.add_option("--node-id", config.node_id, "Current node id")->required();

  // Timing
  app.add_option("--election-timeout", config.election_timeout, "Election timeout interval")
      ->default_val("150ms-300ms");
  app.add_option("--heart-beat-period", config.heart_beat_period, "Leader heart beat period")
      ->default_val("100ms");
  app.add_option("--rpc-timeout", config.rpc_timeout, "Leader heart beat period")
      ->default_val("80ms");

  // Database
  std::filesystem::path storage_path_prefix;
  app.add_option("--store-path", storage_path_prefix, "Raft storage path prefix")
      ->default_val("/tmp/raft_storage/");

  app.set_config("--config", "", "Read toml config");
  app.allow_config_extras(false);

  CLI11_PARSE(app, argc, argv);

  config.raft_state_db_path = storage_path_prefix / "state/";
  config.log_db_path = storage_path_prefix / "log/";

  ceq::raft::test::LoggingStateMachine rsm;
  auto status = ceq::raft::RunMain(&rsm, config);
  if (status.HasError()) {
    std::cerr << status.GetError() << std::endl;
    return 1;
  }
  return 0;
}
