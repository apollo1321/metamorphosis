#pragma once

#include <chrono>
#include <random>
#include <string>
#include <utility>

#include <runtime/rpc_server.h>
#include <runtime/time.h>

namespace ceq::rt::sim {

/*
 * If not all fibers are joined (host was killed, paused or iteration_count is set), there is
 * expected memory leak. Run gtests with main() and then use _Exit() instead of return.
 */

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

  // TODO
  /* std::pair<Duration, Duration> cancellation_lag_interval; */

  // TODO
  /* std::pair<Duration, Duration> kv_store_write_time_interval; */
  /* std::pair<Duration, Duration> kv_store_read_time_interval; */
};

struct WorldOptions {
  std::pair<Duration, Duration> delivery_time_interval;
  double network_error_proba = 0.;
};

void InitWorld(uint64_t seed, WorldOptions options = WorldOptions{}) noexcept;

void AddHost(const Address& address, IHostRunnable* server_main,
             const HostOptions& options = HostOptions{}) noexcept;

void RunSimulation(Duration duration = Duration::max()) noexcept;

////////////////////////////////////////////////////////////
// Helper functions for tests
////////////////////////////////////////////////////////////

uint64_t GetHostUniqueId() noexcept;
Timestamp GetGlobalTime() noexcept;
void FlushAllLogs() noexcept;

////////////////////////////////////////////////////////////
// Failure simulation
////////////////////////////////////////////////////////////

void PauseHost(const Address& address) noexcept;
void ResumeHost(const Address& address) noexcept;

void KillHost(const Address& address) noexcept;
void StartHost(const Address& address) noexcept;

void CloseLink(const Address& from, const Address& to) noexcept;
void RestoreLink(const Address& from, const Address& to) noexcept;

}  // namespace ceq::rt::sim
