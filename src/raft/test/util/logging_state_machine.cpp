#include "logging_state_machine.h"

#include <runtime/util/proto/conversion.h>

#include <raft/test/util/logging_state_machine.pb.h>

namespace mtf::raft::test {

google::protobuf::Any LoggingStateMachine::Apply(const google::protobuf::Any& command) noexcept {
  log.push_back(rt::proto::FromAny<RsmCommand>(command).GetValue().data());

  LOG_DBG("TestStateMachine: command = {}", log.back());

  RsmResult result;
  for (uint64_t val : log) {
    result.add_log_entries(val);
  }

  LOG_DBG("TestStateMachine: result = {}", result);

  return rt::proto::ToAny(result).GetValue();
}

}  // namespace mtf::raft::test
