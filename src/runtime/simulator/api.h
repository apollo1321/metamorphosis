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
  Duration min_start_time = Duration::zero();
  Duration max_start_time = Duration::zero();

  // drift per mcs
  double min_drift = 0.;
  double max_drift = 0.;

  Duration max_sleep_lag = Duration::zero();
};

struct WorldOptions {
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

}  // namespace ceq::rt
