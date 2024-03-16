#pragma once

#include <chrono>

namespace mtf::rt {

using Duration = std::chrono::microseconds;
using Timestamp = std::chrono::time_point<std::chrono::steady_clock, Duration>;

struct Interval {
  Duration from;
  Duration to;
};

}  // namespace mtf::rt
