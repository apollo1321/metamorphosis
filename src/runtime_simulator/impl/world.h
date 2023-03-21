#pragma once

#include <chrono>
#include <random>
#include <unordered_map>
#include <vector>

#include <boost/fiber/all.hpp>

#include <runtime_simulator/api.h>

#include "event.h"
#include "host.h"

namespace runtime_simulation {

class World {
 public:
  void Initialize(uint64_t seed) noexcept;

  std::mt19937& GetGenerator() noexcept;

  Timestamp GlobalTime() const noexcept;

  void AddHost(const Address& address, HostPtr host) noexcept;
  void NotifyHostFinish() noexcept;

  void AddEvent(Timestamp wake_up_time, Event& event) noexcept;

  void RunSimulation() noexcept;

 private:
  std::unordered_map<Address, HostPtr> hosts_;

  bool initialized_ = false;

  std::vector<std::pair<Timestamp, Event*>> events_queue_;

  Timestamp current_time_{};

  size_t running_count_ = 0;

  std::mt19937 generator_;
};

World* GetWorld() noexcept;

}  // namespace runtime_simulation
