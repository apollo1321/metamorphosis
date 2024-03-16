#include <runtime/random.h>

namespace mtf::rt {

std::mt19937& GetGenerator() noexcept {
  static thread_local std::mt19937 generator(42);
  return generator;
}

}  // namespace mtf::rt
