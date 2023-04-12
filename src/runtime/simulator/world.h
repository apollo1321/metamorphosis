#pragma once

#include <chrono>
#include <map>
#include <random>
#include <set>
#include <unordered_map>

#include <boost/fiber/all.hpp>

#include <runtime/util/event.h>

#include "host.h"
#include "rpc_client_base.h"

namespace ceq::rt::sim {

class World {
 public:
  void Initialize(uint64_t seed, WorldOptions options) noexcept;

  std::mt19937& GetGenerator() noexcept;

  Timestamp GetGlobalTime() const noexcept;

  void AddHost(const Address& address, HostPtr host) noexcept;
  void NotifyHostFinish() noexcept;

  void SleepUntil(Timestamp wake_up_time, StopToken stop_token = StopToken{}) noexcept;

  void RunSimulation(Duration duration) noexcept;

  Result<rpc::SerializedData, rpc::Error> MakeRequest(Address from, rpc::Endpoint endpoint,
                                                      rpc::SerializedData data,
                                                      rpc::ServiceName service_name,
                                                      rpc::HandlerName handler_name,
                                                      StopToken stop_token) noexcept;

  Host* GetHost(const Address& address) noexcept;

  void CloseLink(const Address& from, const Address& to) noexcept;
  void RestoreLink(const Address& from, const Address& to) noexcept;

  void FlushAllLogs() noexcept;

 private:
  Duration GetRpcDelay() noexcept;
  bool ShouldMakeNetworkError() noexcept;

 private:
  std::unordered_map<Address, HostPtr> hosts_;

  bool initialized_ = false;

  std::multimap<Timestamp, Event*> events_queue_;

  Timestamp current_time_{};

  size_t running_count_ = 0;

  std::mt19937 generator_;

  WorldOptions options_;

  std::set<std::pair<Address, Address>> closed_links_;
};

World* GetWorld() noexcept;

}  // namespace ceq::rt::sim
