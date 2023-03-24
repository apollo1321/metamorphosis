#pragma once

#include <chrono>
#include <random>

namespace ceq::rt {

using Duration = std::chrono::microseconds;
using Timestamp = std::chrono::time_point<std::chrono::steady_clock, Duration>;

Timestamp Now() noexcept;
void SleepFor(Duration duration) noexcept;
void SleepUntil(Timestamp timestamp) noexcept;

std::mt19937& GetGenerator() noexcept;

}  // namespace ceq::rt
