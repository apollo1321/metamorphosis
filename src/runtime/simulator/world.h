#pragma once

#include <chrono>
#include <map>
#include <random>
#include <unordered_map>

#include <boost/fiber/all.hpp>

#include <runtime/event.h>
#include <runtime/simulator/rpc_client_base.h>

#include "host.h"

namespace ceq::rt {

class World {
 public:
  void Initialize(uint64_t seed, WorldOptions options) noexcept;

  std::mt19937& GetGenerator() noexcept;

  Timestamp GetGlobalTime() const noexcept;

  void AddHost(const Address& address, HostPtr host) noexcept;
  void NotifyHostFinish() noexcept;

  void SleepUntil(Timestamp wake_up_time, StopToken stop_token = StopToken{}) noexcept;

  void RunSimulation(size_t iteration_count) noexcept;

  RpcResult MakeRequest(Endpoint endpint, SerializedData data, ServiceName service_name,
                        HandlerName handler_name, StopToken stop_token) noexcept;

  Host* GetHost(const Address& address) noexcept;

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
};

World* GetWorld() noexcept;

}  // namespace ceq::rt
