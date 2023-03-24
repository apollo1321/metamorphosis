#pragma once

#include <chrono>
#include <random>
#include <unordered_map>
#include <vector>

#include <boost/fiber/all.hpp>

#include <runtime/simulator/rpc_client_base.h>

#include "event.h"
#include "host.h"

namespace ceq::rt {

class World {
 public:
  void Initialize(uint64_t seed, WorldOptions options) noexcept;

  std::mt19937& GetGenerator() noexcept;

  Timestamp GlobalTime() const noexcept;

  void AddHost(const Address& address, HostPtr host) noexcept;
  void NotifyHostFinish() noexcept;

  void SleepUntil(Timestamp wake_up_time) noexcept;

  void RunSimulation() noexcept;

  RpcResult MakeRequest(const Endpoint& endpint, const SerializedData& data,
                        const ServiceName& service_name, const HandlerName& handler_name) noexcept;

 private:
  std::unordered_map<Address, HostPtr> hosts_;

  bool initialized_ = false;

  std::vector<std::pair<Timestamp, Event*>> events_queue_;

  Timestamp current_time_{};

  size_t running_count_ = 0;

  std::mt19937 generator_;

  WorldOptions options_;
};

World* GetWorld() noexcept;

}  // namespace ceq::rt
