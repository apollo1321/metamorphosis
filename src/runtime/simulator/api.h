#pragma once

#include <chrono>
#include <random>
#include <string>

#include <runtime/api.h>
#include <runtime/rpc_server.h>

#include "common.h"

namespace ceq::rt {

struct IHostRunnable {
  virtual void Main() noexcept = 0;
};

struct HostOptions {
  // skew
  std::pair<Duration, Duration> start_time_interval;

  // drift per mcs
  std::pair<double, double> drift_interval;

  Duration max_sleep_lag = Duration::zero();
};

struct WorldOptions {
  // delivery time
  std::pair<Duration, Duration> delivery_time_interval;

  // network errors
  double network_error_proba = 0.;
};

void InitWorld(uint64_t seed, WorldOptions options = WorldOptions{}) noexcept;

std::mt19937& GetGenerator() noexcept;

uint64_t GetHostUniqueId() noexcept;

void AddHost(const Address& address, IHostRunnable* server_main,
             const HostOptions& options = HostOptions{}) noexcept;

void RunSimulation(size_t iteration_count = std::numeric_limits<size_t>::max()) noexcept;

}  // namespace ceq::rt
