#pragma once

#include <runtime/util/chrono/chrono.h>

#include <random>

using namespace std::chrono_literals;

namespace mtf::rt {

struct BackoffParams {
  Duration initial = 10ms;
  Duration max = 1s;
  double factor = 2.0;
};

// https://aws.amazon.com/ru/blogs/architecture/exponential-backoff-and-jitter/
// Implementation of full jitter
struct Backoff {
 public:
  Backoff(BackoffParams params, std::mt19937& generator) noexcept;

  Duration Next() noexcept;

 private:
  BackoffParams params_;
  Duration current_{};

  std::mt19937& generator_;
};

}  // namespace mtf::rt
