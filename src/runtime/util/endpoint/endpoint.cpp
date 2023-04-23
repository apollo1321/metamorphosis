#include "endpoint.h"

namespace ceq::rt {

std::string Endpoint::ToString() const noexcept {
  return address + ":" + std::to_string(port);
}

}  // namespace ceq::rt
