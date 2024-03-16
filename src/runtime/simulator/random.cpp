#include <runtime/random.h>

#include "world.h"

namespace mtf::rt {

std::mt19937& GetGenerator() noexcept {
  return sim::GetWorld()->GetGenerator();
}

}  // namespace mtf::rt
