#include <runtime/random.h>

#include "world.h"

namespace ceq::rt {

std::mt19937& GetGenerator() noexcept {
  return sim::GetWorld()->GetGenerator();
}

}  // namespace ceq::rt
