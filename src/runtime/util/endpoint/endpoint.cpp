#include "endpoint.h"

namespace ceq::rt {

Endpoint::Endpoint(Address address, Port port) noexcept : address{std::move(address)}, port{port} {
}

std::string Endpoint::ToString() const noexcept {
  return address + ":" + std::to_string(port);
}

}  // namespace ceq::rt
