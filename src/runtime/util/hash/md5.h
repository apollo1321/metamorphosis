#pragma once

#include <string_view>
#include <array>

namespace mtf::rt {

struct MD5Hash {
  std::array<uint8_t, 16> digest;
};

MD5Hash ComputeMd5Hash(std::string_view data) noexcept;
std::string ToString(const MD5Hash& hash) noexcept;

}  // namespace mtf::rt
