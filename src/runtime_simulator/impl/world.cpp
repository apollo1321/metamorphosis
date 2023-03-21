#include "world.h"
#include "scheduler.h"

#include <algorithm>

#include <util/condition_check.h>

namespace runtime_simulation {

void World::Initialize(uint64_t seed) noexcept {
  generator_ = std::mt19937(seed);
  current_time_ = Timestamp(static_cast<Duration>(0));
  events_queue_.clear();
  hosts_.clear();
  initialized_ = true;
}

std::mt19937& World::GetGenerator() noexcept {
  return generator_;
}

Timestamp World::GlobalTime() const noexcept {
  return current_time_;
}

void World::AddHost(const Address& address, HostPtr host) noexcept {
  ++running_count_;
  VERIFY(!hosts_.contains(address), "address already used");
  hosts_[address] = std::move(host);
}

void World::NotifyHostFinish() noexcept {
  --running_count_;
}

void World::AddEvent(Timestamp wake_up_time, Event& event) noexcept {
  events_queue_.emplace_back(std::make_pair(wake_up_time, &event));
  std::push_heap(events_queue_.begin(), events_queue_.end(), std::greater<>{});
}

void World::RunSimulation() noexcept {
  VERIFY(std::exchange(initialized_, false), "world in unitialized");
  boost::fibers::use_scheduling_algorithm<RuntimeSimulationScheduler>();
  boost::this_fiber::properties<RuntimeSimulationProps>().MarkAsMainFiber();

  while (running_count_ > 0) {
    VERIFY(!events_queue_.empty(), "unexpected state: no active tasks (possible deadlock)");

    std::pop_heap(events_queue_.begin(), events_queue_.end(), std::greater<>{});
    auto [ts, event] = events_queue_.back();
    events_queue_.pop_back();

    VERIFY(current_time_ <= ts, "invalid event timestamp");
    current_time_ = ts;
    event->Signal();

    // this fiber will be resumes after all other fibers are executed
    boost::this_fiber::yield();
  }
}

World* GetWorld() noexcept {
  static World world;
  return &world;
}

}  // namespace runtime_simulation
