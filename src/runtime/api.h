#pragma once

#include <chrono>
#include <random>

#include "cancellation/stop_token.h"

namespace ceq::rt {

using Duration = std::chrono::microseconds;
using Timestamp = std::chrono::time_point<std::chrono::steady_clock, Duration>;

Timestamp Now() noexcept;
bool SleepFor(Duration duration, StopToken stop_token = StopToken{}) noexcept;
bool SleepUntil(Timestamp timestamp, StopToken stop_token = StopToken{}) noexcept;

std::mt19937& GetGenerator() noexcept;

}  // namespace ceq::rt
