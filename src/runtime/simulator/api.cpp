#include "api.h"

#include <util/condition_check.h>

#include "world.h"

namespace ceq::rt {

Timestamp Now() noexcept {
  return GetCurrentHost()->GetLocalTime();
}

void SleepFor(Duration duration) noexcept {
  SleepUntil(Now() + duration);
}

void SleepUntil(Timestamp timestamp) noexcept {
  GetCurrentHost()->SleepUntil(timestamp);
}

void InitWorld(uint64_t seed, WorldOptions options) noexcept {
  GetWorld()->Initialize(seed, options);
}

std::mt19937& GetGenerator() noexcept {
  return GetWorld()->GetGenerator();
}

void AddHost(const Address& address, IHostRunnable* host_main,
             const HostOptions& options) noexcept {
  GetWorld()->AddHost(address, std::make_unique<Host>(host_main, options));
}

void RunSimulation() noexcept {
  GetWorld()->RunSimulation();
}

}  // namespace ceq::rt
