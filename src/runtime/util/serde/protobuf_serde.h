#pragma once

#include <google/protobuf/message.h>
#include <runtime/kv_storage.h>

namespace ceq::rt::kv {

template <class Proto>
struct ProtobufSerde {
  db::Data Serialize(const google::protobuf::Message& value) const noexcept {
    db::Data result;
    result.resize(value.ByteSizeLong());
    VERIFY(value.SerializeToArray(result.data(), result.size()), "serialization error");
    return result;
  }

  Proto Deserialize(db::DataView data) const noexcept {
    Proto result;
    VERIFY(result.ParseFromArray(data.data(), data.size()), "deserialization error");
    return result;
  }
};

}  // namespace ceq::rt::kv
