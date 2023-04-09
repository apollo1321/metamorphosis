#include "api.h"
#include "world.h"

namespace ceq::rt {

Timestamp Now() noexcept {
  return GetCurrentHost()->GetLocalTime();
}

bool SleepFor(Duration duration, StopToken stop_token) noexcept {
  return SleepUntil(Now() + duration, std::move(stop_token));
}

bool SleepUntil(Timestamp timestamp, StopToken stop_token) noexcept {
  return GetCurrentHost()->SleepUntil(timestamp, std::move(stop_token));
}

void InitWorld(uint64_t seed, WorldOptions options) noexcept {
  GetWorld()->Initialize(seed, options);
}

std::mt19937& GetGenerator() noexcept {
  return GetWorld()->GetGenerator();
}

void AddHost(const Address& address, IHostRunnable* host_main,
             const HostOptions& options) noexcept {
  GetWorld()->AddHost(address, std::make_unique<Host>(address, host_main, options));
}

void RunSimulation(size_t iteration_count) noexcept {
  GetWorld()->RunSimulation(iteration_count);
}

uint64_t GetHostUniqueId() noexcept {
  return reinterpret_cast<uint64_t>(GetCurrentHost());
}

std::shared_ptr<spdlog::logger> GetLogger() noexcept {
  return GetCurrentHost()->GetLogger();
}

void PauseHost(const Address& address) noexcept {
  GetWorld()->GetHost(address)->PauseHost();
}

void ResumeHost(const Address& address) noexcept {
  GetWorld()->GetHost(address)->ResumeHost();
}

}  // namespace ceq::rt
