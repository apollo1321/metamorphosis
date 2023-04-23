#pragma once

#include <cstdint>
#include <string>

namespace ceq::rt {

using Port = uint16_t;
using Address = std::string;

struct Endpoint {
  std::string ToString() const noexcept;

  Address address;
  Port port;
};

}  // namespace ceq::rt
