#pragma once

#include "serde.h"

#include <string>

namespace mtf::rt::serde {

struct StringSerde {
  DataView Serialize(const std::string& value) const noexcept {
    return DataView(reinterpret_cast<const uint8_t*>(value.data()), value.size());
  }

  std::string Deserialize(DataView data) const noexcept {
    return std::string(data.begin(), data.end());
  }
};

static_assert(CSerde<StringSerde>);

}  // namespace mtf::rt::serde
