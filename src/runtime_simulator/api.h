#pragma once

#include <chrono>
#include <random>
#include <string>

namespace runtime_simulation {

using Address = std::string;

using Duration = std::chrono::microseconds;
using Timestamp = std::chrono::time_point<Duration>;

// API

Timestamp now() noexcept;                        // NOLINT
void sleep_for(Duration duration) noexcept;      // NOLINT
void sleep_until(Timestamp timestamp) noexcept;  // NOLINT

struct IHostRunnable {
  // Will be runned in fiber
  virtual void operator()() = 0;
};

struct HostOptions {
  // clock skew
  Duration min_start_time = Duration::zero();
  Duration max_start_time = Duration::zero();

  // clock drift per mcs
  double min_drift = 0.;
  double max_drift = 0.;

  Duration max_sleep_lag = Duration::zero();
};

void InitWorld(uint64_t seed) noexcept;

std::mt19937& GetGenerator() noexcept;

void AddHost(const Address& address, IHostRunnable* server_main,
             const HostOptions& options = HostOptions{}) noexcept;

void RunSimulation() noexcept;

}  // namespace runtime_simulation
