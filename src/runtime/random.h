#pragma once

#include <runtime/util/chrono/chrono.h>

#include <random>

namespace ceq::rt {

std::mt19937& GetGenerator() noexcept;

double GetProbability() noexcept;
Duration GetRandomDuration(const Interval& interval) noexcept;
double GetRandomFloat(double from, double to) noexcept;

}  // namespace ceq::rt
