#pragma once

#include <raft/node/node.h>

#include <google/protobuf/any.h>

#include <vector>

namespace mtf::raft::test {

/**
 * Implementation of the simple state machine that stores client command and returns its full log
 * with newly added client command.
 */
struct LoggingStateMachine final : public mtf::raft::IStateMachine {
  google::protobuf::Any Apply(const google::protobuf::Any& command) noexcept override;

  std::vector<uint64_t> log;
};

}  // namespace mtf::raft::test
