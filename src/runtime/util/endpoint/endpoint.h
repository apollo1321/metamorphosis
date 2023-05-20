#pragma once

#include <cstdint>
#include <string>

namespace ceq::rt {

using Port = uint16_t;
using Address = std::string;

struct Endpoint {
  Endpoint() noexcept = default;
  Endpoint(Address address, Port port) noexcept;

  std::string ToString() const noexcept;

  Address address;
  Port port{};
};

}  // namespace ceq::rt
