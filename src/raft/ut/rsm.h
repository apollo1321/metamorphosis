#pragma once

#include <raft/node.h>
#include <raft/ut/test_rsm.pb.h>

namespace ceq::raft::test {

template <class T>
google::protobuf::Any ToAny(const T& proto) noexcept {
  google::protobuf::Any result;
  VERIFY(result.PackFrom(proto), "broken protobuf");
  return result;
}

template <class T>
T FromAny(const google::protobuf::Any& proto) noexcept {
  T result;
  VERIFY(proto.UnpackTo(&result), "broken protobuf");
  return result;
}

struct TestStateMachine final : public ceq::raft::IStateMachine {
  google::protobuf::Any Apply(const google::protobuf::Any& command) noexcept override {
    log.push_back(FromAny<RsmCommand>(command).data());

    LOG("TestStateMachine: command = {}", log.back());

    RsmResult result;
    for (uint64_t val : log) {
      result.add_log_entries(val);
    }

    LOG("TestStateMachine: result = {}", result);

    return ToAny(result);
  }

  std::vector<uint64_t> log;
};

}  // namespace ceq::raft::test
