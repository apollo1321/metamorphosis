#pragma once

#include <vector>

#include <runtime/rpc_server.h>

namespace ceq::raft {

using Cluster = std::vector<rt::rpc::Endpoint>;

}
