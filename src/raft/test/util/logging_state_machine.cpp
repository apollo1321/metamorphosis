#include "logging_state_machine.h"

#include <runtime/util/proto/proto_conversion.h>

#include <raft/test/util/logging_state_machine.pb.h>

namespace ceq::raft::test {

google::protobuf::Any LoggingStateMachine::Apply(const google::protobuf::Any& command) noexcept {
  log.push_back(rt::proto::FromAny<RsmCommand>(command).GetValue().data());

  LOG("TestStateMachine: command = {}", log.back());

  RsmResult result;
  for (uint64_t val : log) {
    result.add_log_entries(val);
  }

  LOG("TestStateMachine: result = {}", result);

  return rt::proto::ToAny(result).GetValue();
}

}  // namespace ceq::raft::test
