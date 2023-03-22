#pragma once

#include <chrono>
#include <random>
#include <string>

#include "common.h"

namespace runtime_simulation {

Timestamp now() noexcept;                        // NOLINT
void sleep_for(Duration duration) noexcept;      // NOLINT
void sleep_until(Timestamp timestamp) noexcept;  // NOLINT

struct IHostRunnable {
  // Will be runned in fiber
  virtual void operator()() = 0;
};

struct HostOptions {
  /// Clock options
  // skew
  Duration min_start_time = Duration::zero();
  Duration max_start_time = Duration::zero();

  // drift per mcs
  double min_drift = 0.;
  double max_drift = 0.;

  Duration max_sleep_lag = Duration::zero();
};

struct WorldOptions {
  /// Network options
  // delivery time
  Duration min_delivery_time = Duration::zero();
  Duration max_delivery_time = Duration::zero();

  // network errors
  double network_error_proba = 0.;
};

void InitWorld(uint64_t seed, WorldOptions options = WorldOptions{}) noexcept;

std::mt19937& GetGenerator() noexcept;

void AddHost(const Address& address, IHostRunnable* server_main,
             const HostOptions& options = HostOptions{}) noexcept;

void RunSimulation() noexcept;

}  // namespace runtime_simulation
