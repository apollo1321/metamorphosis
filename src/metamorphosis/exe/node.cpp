#include <metamorphosis/node/node.h>

#include <runtime/util/parse/parse.h>

#include <CLI/CLI.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace mtf::rt;  // NOLINT

int main(int argc, char** argv) {
  CLI::App app{"Metamorphosis node"};

  mtf::mtf::NodeConfig config;

  // Nodes
  app.add_option("--nodes", config.cluster_nodes, "Metamorphosis nodes endpoints, addr:port")
      ->required();
  app.add_option("--node-id", config.node_id, "Current node id")->required();

  // Timing
  app.add_option("--election-timeout", config.election_timeout, "Election timeout interval")
      ->default_val("150ms-300ms");
  app.add_option("--heart-beat-period", config.heart_beat_period, "Leader heart beat period")
      ->default_val("100ms");
  app.add_option("--rpc-timeout", config.rpc_timeout, "Timeout for internal RPC's")
      ->default_val("80ms");

  // Database
  std::filesystem::path storage_path_prefix;
  app.add_option("--store-path", storage_path_prefix, "Metamorphosis storage path prefix")
      ->default_val("/tmp/mtf_storage/");

  app.set_config("--config", "", "Read toml config");
  app.allow_config_extras(false);

  CLI11_PARSE(app, argc, argv);

  config.state_db_path = storage_path_prefix / "state/";
  config.log_db_path = storage_path_prefix / "log/";

  auto status = mtf::mtf::RunMain(config);
  if (status.HasError()) {
    std::cerr << status.GetError() << std::endl;
    return 1;
  }
  return 0;
}
