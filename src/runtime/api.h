#pragma once

#include <chrono>
#include <memory>
#include <random>

#include <runtime/cancellation/stop_token.h>
#include <spdlog/spdlog.h>

namespace ceq::rt {

using Duration = std::chrono::microseconds;
using Timestamp = std::chrono::time_point<std::chrono::steady_clock, Duration>;

Timestamp Now() noexcept;
bool SleepFor(Duration duration, StopToken stop_token = StopToken{}) noexcept;
bool SleepUntil(Timestamp timestamp, StopToken stop_token = StopToken{}) noexcept;

std::mt19937& GetGenerator() noexcept;

std::shared_ptr<spdlog::logger> GetLogger() noexcept;

}  // namespace ceq::rt
