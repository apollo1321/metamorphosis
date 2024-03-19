#pragma once

#include <utility>
#include <vector>

#include <google/protobuf/any.pb.h>

#include <runtime/api.h>

namespace mtf::mtf {

struct NodeConfig {
  size_t node_id{};
  std::vector<rt::Endpoint> cluster_nodes;

  rt::Interval election_timeout;
  rt::Duration heart_beat_period;
  rt::Duration rpc_timeout;

  rt::Duration fallback_timeout;

  std::filesystem::path log_db_path;
  std::filesystem::path state_db_path;
};

Status<std::string> RunMain(NodeConfig config) noexcept;

}  // namespace mtf::mtf
