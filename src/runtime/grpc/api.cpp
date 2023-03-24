#include <runtime/api.h>

#include <boost/fiber/operations.hpp>

namespace ceq::rt {

Timestamp Now() noexcept {
  return std::chrono::time_point_cast<Duration>(std::chrono::steady_clock::now());
}

void SleepFor(Duration duration) noexcept {
  boost::this_fiber::sleep_for(duration);
}

void SleepUntil(Timestamp timestamp) noexcept {
  boost::this_fiber::sleep_until(timestamp);
}

std::mt19937& GetGenerator() noexcept {
  static thread_local std::mt19937 generator(42);
  return generator;
}

}  // namespace ceq::rt
