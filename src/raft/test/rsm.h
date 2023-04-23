#pragma once

#include <raft/node/node.h>
#include <raft/test/rsm_msg.pb.h>

#include <runtime/util/proto/proto_conversion.h>

namespace ceq::raft::test {

struct TestStateMachine final : public ceq::raft::IStateMachine {
  google::protobuf::Any Apply(const google::protobuf::Any& command) noexcept override {
    log.push_back(rt::proto::FromAny<RsmCommand>(command).GetValue().data());

    LOG("TestStateMachine: command = {}", log.back());

    RsmResult result;
    for (uint64_t val : log) {
      result.add_log_entries(val);
    }

    LOG("TestStateMachine: result = {}", result);

    return rt::proto::ToAny(result).GetValue();
  }

  std::vector<uint64_t> log;
};

}  // namespace ceq::raft::test
