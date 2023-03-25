#include "world.h"
#include "scheduler.h"

#include <algorithm>

#include <runtime/cancellation/stop_callback.h>
#include <util/condition_check.h>

namespace ceq::rt {

void World::Initialize(uint64_t seed, WorldOptions options) noexcept {
  boost::fibers::use_scheduling_algorithm<RuntimeSimulationScheduler>();
  options_ = options;
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

void World::SleepUntil(Timestamp wake_up_time, StopToken stop_token) noexcept {
  Event event;
  auto it = events_queue_.insert(std::make_pair(wake_up_time, &event));
  StopCallback stop_guard(stop_token, [&]() {
    events_queue_.erase(it);
    event.Signal();
  });
  event.Await();
}

void World::RunSimulation() noexcept {
  VERIFY(std::exchange(initialized_, false), "world in unitialized");
  boost::this_fiber::properties<RuntimeSimulationProps>().MarkAsMainFiber();
  boost::this_fiber::yield();

  while (running_count_ > 0) {
    VERIFY(!events_queue_.empty(),
           "unexpected state: no active tasks (deadlock or real sleep_for is called)");

    auto [ts, event] = *events_queue_.begin();
    events_queue_.erase(events_queue_.begin());

    VERIFY(current_time_ <= ts, "invalid event timestamp");
    current_time_ = ts;
    event->Signal();

    // this fiber will be resumed after all other fibers are executed
    boost::this_fiber::yield();
  }
}

RpcResult World::MakeRequest(const Endpoint& endpoint, const SerializedData& data,
                             const ServiceName& service_name,
                             const HandlerName& handler_name) noexcept {
  using Result = Result<SerializedData, RpcError>;

  std::uniform_int_distribution<Duration::rep> delay_dist(options_.min_delivery_time.count(),
                                                          options_.max_delivery_time.count());
  std::uniform_real_distribution<double> prob_dist(0., 1.);

  SleepUntil(GlobalTime() + Duration(delay_dist(GetGenerator())));

  if (!hosts_.contains(endpoint.address)) {
    return RpcResult::Err(RpcError::ErrorType::HostNotFound);
  }

  auto result =
      hosts_[endpoint.address]->ProcessRequest(endpoint.port, data, service_name, handler_name);

  SleepUntil(GlobalTime() + Duration(delay_dist(GetGenerator())));

  if (prob_dist(GetGenerator()) < options_.network_error_proba) {
    return Result::Err(RpcError::ErrorType::NetworkError);
  }

  return result;
}

World* GetWorld() noexcept {
  static World world;
  return &world;
}

}  // namespace ceq::rt
