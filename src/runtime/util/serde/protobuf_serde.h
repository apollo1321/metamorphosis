#pragma once

#include "serde.h"

#include <util/condition_check.h>

#include <google/protobuf/message.h>

namespace ceq::rt::serde {

template <class Proto>
struct ProtobufSerde {
  Data Serialize(const google::protobuf::Message& value) const noexcept {
    Data result;
    result.resize(value.ByteSizeLong());
    VERIFY(value.SerializeToArray(result.data(), result.size()), "serialization error");
    return result;
  }

  Proto Deserialize(DataView data) const noexcept {
    Proto result;
    VERIFY(result.ParseFromArray(data.data(), data.size()), "deserialization error");
    return result;
  }
};

}  // namespace ceq::rt::serde
