#pragma once

#include <runtime/util/chrono/chrono.h>

#include <limits>
#include <random>

namespace mtf::rt {

std::mt19937& GetGenerator() noexcept;

double GetProbability() noexcept;
Duration GetRandomDuration(const Interval& interval) noexcept;
double GetRandomFloat(double from, double to) noexcept;
uint64_t GetRandomInt(uint64_t from = 0,
                      uint64_t to = std::numeric_limits<uint64_t>::max()) noexcept;

}  // namespace mtf::rt
