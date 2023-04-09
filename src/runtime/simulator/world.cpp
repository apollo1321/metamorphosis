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

Timestamp World::GetGlobalTime() const noexcept {
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

void World::RunSimulation(size_t iteration_count) noexcept {
  VERIFY(std::exchange(initialized_, false), "world in unitialized");
  boost::this_fiber::properties<RuntimeSimulationProps>().MarkAsMainFiber();
  boost::this_fiber::yield();

  while (running_count_ > 0 && iteration_count > 0) {
    VERIFY(!events_queue_.empty(),
           "unexpected state: no active tasks (deadlock or real sleep_for is called)");

    auto [ts, event] = *events_queue_.begin();
    events_queue_.erase(events_queue_.begin());

    VERIFY(current_time_ <= ts, "invalid event timestamp");
    current_time_ = ts;
    event->Signal();

    // this fiber will be resumed after all other fibers are executed
    boost::this_fiber::yield();

    --iteration_count;
  }
}

Duration World::GetRpcDelay() noexcept {
  std::uniform_int_distribution<Duration::rep> delay_dist(
      options_.delivery_time_interval.first.count(),
      options_.delivery_time_interval.second.count());
  return Duration(delay_dist(GetGenerator()));
}

bool World::ShouldMakeNetworkError() noexcept {
  std::uniform_real_distribution<double> prob_dist(0., 1.);

  return prob_dist(GetGenerator()) < options_.network_error_proba;
}

RpcResult World::MakeRequest(Endpoint endpoint, SerializedData data, ServiceName service_name,
                             HandlerName handler_name, StopToken stop_token) noexcept {
  struct State {
    RpcResult result = Err(RpcError::ErrorType::Cancelled);
    Event event;
  };

  auto state = std::make_shared<State>();

  StopCallback stop_callback(stop_token, [&]() {
    state->event.Signal();
  });

  boost::fibers::fiber([this, endpoint = std::move(endpoint), data = std::move(data),
                        service_name = std::move(service_name),
                        handler_name = std::move(handler_name), state, stop_token]() {
    SleepUntil(GetGlobalTime() + GetRpcDelay(), stop_token);

    if (!hosts_.contains(endpoint.address)) {
      state->result = Err(RpcError::ErrorType::ConnectionRefused);
      state->event.Signal();
      return;
    }

    auto target_host = hosts_[endpoint.address].get();

    boost::this_fiber::properties<RuntimeSimulationProps>().SetCurrentHost(
        target_host, target_host->GetCurrentEpoch());

    if (stop_token.StopRequested()) {
      return;
    }

    auto result = target_host->ProcessRequest(endpoint.port, data, service_name, handler_name);

    SleepUntil(GetGlobalTime() + GetRpcDelay(), stop_token);

    if (ShouldMakeNetworkError()) {
      state->result = Err(RpcError::ErrorType::NetworkError);
    } else {
      state->result = std::move(result);
    }
    state->event.Signal();
  }).detach();

  state->event.Await();

  return std::move(state->result);
}

Host* World::GetHost(const Address& address) noexcept {
  auto it = hosts_.find(address);
  VERIFY(it != hosts_.end(), "host <" + address + "> is not registered in the world");
  return it->second.get();
}

World* GetWorld() noexcept {
  static World world;
  return &world;
}

}  // namespace ceq::rt
