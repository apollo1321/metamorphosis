#include "api.h"
#include "world.h"

namespace mtf::rt::sim {

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

void RunSimulation(Duration duration, size_t iteration_count) noexcept {
  GetWorld()->RunSimulation(duration, iteration_count);
}

uint64_t GetHostUniqueId() noexcept {
  return reinterpret_cast<uint64_t>(GetCurrentHost());
}

Timestamp GetGlobalTime() noexcept {
  return GetWorld()->GetGlobalTime();
}

void FlushAllLogs() noexcept {
  GetWorld()->FlushAllLogs();
}

void PauseHost(const Address& address) noexcept {
  GetWorld()->GetHost(address)->PauseHost();
}

void ResumeHost(const Address& address) noexcept {
  GetWorld()->GetHost(address)->ResumeHost();
}

void KillHost(const Address& address) noexcept {
  GetWorld()->GetHost(address)->KillHost();
}

void StartHost(const Address& address) noexcept {
  GetWorld()->GetHost(address)->StartHost();
}

void CloseLink(const Address& from, const Address& to) noexcept {
  GetWorld()->CloseLink(from, to);
}

void CloseLinkBidirectional(const Address& first, const Address& second) noexcept {
  CloseLink(first, second);
  CloseLink(second, first);
}

void RestoreLink(const Address& from, const Address& to) noexcept {
  GetWorld()->RestoreLink(from, to);
}

void RestoreLinkBidirectional(const Address& first, const Address& second) noexcept {
  RestoreLink(first, second);
  RestoreLink(second, first);
}

}  // namespace mtf::rt::sim
