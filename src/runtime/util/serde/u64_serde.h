#pragma once

#include <runtime/kv_storage.h>

namespace ceq::rt::kv {

struct U64Serde {
  db::DataView Serialize(const uint64_t& value) const noexcept {
    return db::DataView(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
  }

  uint64_t Deserialize(db::DataView data) const noexcept {
    VERIFY(data.size() == sizeof(uint64_t), "invalid data size");
    return *reinterpret_cast<const uint64_t*>(data.data());
  }
};

static_assert(kv::CSerde<U64Serde>);

}  // namespace ceq::rt::db
