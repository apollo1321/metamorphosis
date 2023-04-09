#pragma once

#include <chrono>
#include <random>
#include <string>

#include <runtime/api.h>
#include <runtime/rpc_server.h>

#include "common.h"

namespace ceq::rt {

////////////////////////////////////////////////////////////
// World start-up
////////////////////////////////////////////////////////////

struct IHostRunnable {
  virtual void Main() noexcept = 0;
};

struct HostOptions {
  std::pair<Duration, Duration> start_time_interval;
  std::pair<double, double> drift_interval;
  Duration max_sleep_lag = Duration::zero();
};

struct WorldOptions {
  std::pair<Duration, Duration> delivery_time_interval;
  double network_error_proba = 0.;
};

void InitWorld(uint64_t seed, WorldOptions options = WorldOptions{}) noexcept;

void AddHost(const Address& address, IHostRunnable* server_main,
             const HostOptions& options = HostOptions{}) noexcept;

void RunSimulation(size_t iteration_count = std::numeric_limits<size_t>::max()) noexcept;

////////////////////////////////////////////////////////////
// Helper functions for tests
////////////////////////////////////////////////////////////

uint64_t GetHostUniqueId() noexcept;
Timestamp GetGlobalTime() noexcept;

////////////////////////////////////////////////////////////
// Failure simulation
////////////////////////////////////////////////////////////

void PauseHost(const Address& address) noexcept;
void ResumeHost(const Address& address) noexcept;

// TODO
/* void KillHost(const Address& address) noexcept; */
/* void StartHost(const Address& address) noexcept; */

/* void DropLink(const Address& from, const Address& to) noexcept; */
/* void RestoreLink(const Address& from, const Address& to) noexcept; */

}  // namespace ceq::rt
