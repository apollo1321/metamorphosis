#pragma once

#include <random>

namespace ceq::rt {

std::mt19937& GetGenerator() noexcept;

}  // namespace ceq::rt
