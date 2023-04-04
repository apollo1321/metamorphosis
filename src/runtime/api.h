#pragma once

#include <chrono>
#include <memory>
#include <ostream>
#include <random>

#include <google/protobuf/message.h>

// clang-format off
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h> // should be included after spdlog.h
// clang-format on

#include <runtime/cancellation/stop_token.h>

#define LOG(...) SPDLOG_LOGGER_INFO(ceq::rt::GetLogger(), __VA_ARGS__)

namespace ceq::rt {

using Duration = std::chrono::microseconds;
using Timestamp = std::chrono::time_point<std::chrono::steady_clock, Duration>;

Timestamp Now() noexcept;
bool SleepFor(Duration duration, StopToken stop_token = StopToken{}) noexcept;
bool SleepUntil(Timestamp timestamp, StopToken stop_token = StopToken{}) noexcept;

std::mt19937& GetGenerator() noexcept;

std::shared_ptr<spdlog::logger> GetLogger() noexcept;

}  // namespace ceq::rt

// Print proto messages
std::ostream& operator<<(std::ostream& os, const google::protobuf::Message& proto);
