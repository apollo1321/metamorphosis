#include <iostream>

#include <runtime/util/parse/parse.h>

#include <CLI/CLI.hpp>

#include <raft/node/node.h>
#include <raft/test/rsm.h>
#include <raft/test/rsm_msg.pb.h>

using namespace ceq::rt;  // NOLINT

int main(int argc, char** argv) {
  CLI::App app{"raft client"};

  ceq::raft::RaftConfig config;

  // Nodes
  app.add_option("--raft-nodes", config.raft_nodes, "raft nodes endpoints, addr:port")->required();
  app.add_option("--node-id", config.node_id, "current node id")->required();

  // Timing
  app.add_option("--election-timeout", config.election_timeout, "election timeout interval")
      ->default_str("150ms-300ms");
  app.add_option("--heart-beat-period", config.heart_beat_period, "leader heart beat period")
      ->default_str("100ms");
  app.add_option("--rpc-timeout", config.rpc_timeout, "leader heart beat period")
      ->default_str("80ms");

  // Database
  app.add_option("--log-db-path", config.log_db_path, "raft log database path")
      ->default_str("/tmp/raft_log/")
      ->check(CLI::ExistingDirectory);
  app.add_option("--state-db-path", config.raft_state_db_path, "raft state database path")
      ->default_str("/tmp/raft_state/")
      ->check(CLI::ExistingDirectory);

  app.set_config("--config", "", "read toml config");
  app.allow_config_extras(false);

  CLI11_PARSE(app, argc, argv);

  ceq::raft::test::TestStateMachine rsm;
  ceq::raft::RunMain(&rsm, config);

  return 0;
}
