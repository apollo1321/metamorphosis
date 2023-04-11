#pragma once

#include <runtime/kv_storage.h>

namespace ceq::rt::kv {

struct StringSerde {
  db::DataView Serialize(const std::string& value) const noexcept {
    return db::DataView(reinterpret_cast<const uint8_t*>(value.data()), value.size());
  }

  std::string Deserialize(db::DataView data) const noexcept {
    return std::string(data.begin(), data.end());
  }
};

static_assert(kv::CSerde<StringSerde>);

}  // namespace ceq::rt::kv
