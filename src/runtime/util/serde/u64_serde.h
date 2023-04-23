#pragma once

#include "serde.h"

#include <util/condition_check.h>

#include <cstdint>

namespace ceq::rt::serde {

struct U64Serde {
  DataView Serialize(const uint64_t& value) const noexcept {
    return DataView(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
  }

  uint64_t Deserialize(DataView data) const noexcept {
    VERIFY(data.size() == sizeof(uint64_t), "invalid data size");
    return *reinterpret_cast<const uint64_t*>(data.data());
  }
};

static_assert(CSerde<U64Serde>);

}  // namespace ceq::rt::serde
