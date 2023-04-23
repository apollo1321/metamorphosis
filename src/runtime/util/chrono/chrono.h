#pragma once

#include <chrono>

namespace ceq::rt {

using Duration = std::chrono::microseconds;
using Timestamp = std::chrono::time_point<std::chrono::steady_clock, Duration>;

struct Interval {
  Duration from;
  Duration to;
};

}  // namespace ceq::rt
