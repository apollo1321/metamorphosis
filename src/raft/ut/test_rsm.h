#pragma once

#include <raft/node.h>

#include <raft/ut/test_rsm.pb.h>

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
  google::protobuf::Any Execute(const google::protobuf::Any& command) noexcept override {
    log.push_back(FromAny<RsmCommand>(command).data());

    LOG("TestStateMachine: command = {}", log.back());

    RsmResponse response;
    for (uint64_t val : log) {
      response.add_log_entries(val);
    }

    LOG("TestStateMachine: response = {}", response);

    return ToAny(response);
  }

  std::vector<uint64_t> log;
};
