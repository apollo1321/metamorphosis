#include "world.h"
#include "scheduler.h"

#include <algorithm>

#include <runtime/random.h>
#include <runtime/util/cancellation/stop_callback.h>
#include <util/condition_check.h>

namespace ceq::rt::sim {

void World::Initialize(uint64_t seed, WorldOptions options) noexcept {
  boost::fibers::use_scheduling_algorithm<RuntimeSimulationScheduler>();
  options_ = options;
  generator_ = std::mt19937(seed);
  initialized_ = true;
}

std::mt19937& World::GetGenerator() noexcept {
  return generator_;
}

Timestamp World::GetGlobalTime() const noexcept {
  return current_time_;
}

void World::AddHost(const Address& address, HostPtr host) noexcept {
  VERIFY(!hosts_.contains(address), "address already used");
  hosts_[address] = std::move(host);
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

void World::RunSimulation(Duration duration, size_t iteration_count) noexcept {
  VERIFY(initialized_, "world in unitialized");
  boost::this_fiber::properties<RuntimeSimulationProps>().MarkAsMainFiber();
  boost::this_fiber::yield();

  while (!events_queue_.empty() && events_queue_.begin()->first.time_since_epoch() <= duration &&
         iteration_count > 0) {
    --iteration_count;
    auto [ts, event] = *events_queue_.begin();
    events_queue_.erase(events_queue_.begin());

    VERIFY(current_time_ <= ts, "invalid event timestamp");
    current_time_ = ts;
    event->Signal();

    // this fiber will be resumed after all other fibers are executed
    boost::this_fiber::yield();
  }

  FlushAllLogs();
  current_time_ = Timestamp(static_cast<Duration>(0));
  events_queue_.clear();
  for (auto& [addr, host] : hosts_) {
    host.release();
  }
  hosts_.clear();
  closed_links_.clear();
  initialized_ = false;
}

Duration World::GetRpcDelay() noexcept {
  if (GetProbability() < options_.long_delivery_time_proba) {
    return GetRandomDuration(options_.long_delivery_time);
  }
  return GetRandomDuration(options_.delivery_time);
}

bool World::ShouldMakeNetworkError() noexcept {
  std::uniform_real_distribution<double> prob_dist(0., 1.);

  return prob_dist(GetGenerator()) < options_.network_error_proba;
}

Result<rpc::SerializedData, rpc::RpcError> World::MakeRequest(Address from, Endpoint endpoint,
                                                              rpc::SerializedData data,
                                                              rpc::ServiceName service_name,
                                                              rpc::HandlerName handler_name,
                                                              StopToken stop_token) noexcept {
  struct State {
    Result<rpc::SerializedData, rpc::RpcError> result = Err(rpc::RpcErrorType::Cancelled);
    Event event;
  };

  auto state = std::make_shared<State>();

  StopCallback stop_callback(stop_token, [&]() {
    state->event.Signal();
  });

  boost::fibers::fiber([this, from = std::move(from), endpoint = std::move(endpoint),
                        data = std::move(data), service_name = std::move(service_name),
                        handler_name = std::move(handler_name), state, stop_token]() {
    SleepUntil(GetGlobalTime() + GetRpcDelay(), stop_token);

    const bool is_network_error = ShouldMakeNetworkError();

    if (is_network_error && std::uniform_real_distribution<double>(0., 1.)(GetGenerator()) < 0.5) {
      state->result = Err(rpc::RpcErrorType::NetworkError);
      state->event.Signal();
      return;
    }

    if (!hosts_.contains(endpoint.address)) {
      state->result = Err(rpc::RpcErrorType::ConnectionRefused);
      state->event.Signal();
      return;
    }

    if (closed_links_.contains(std::make_pair(from, endpoint.address))) {
      state->result = Err(rpc::RpcErrorType::NetworkError);
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

    if (closed_links_.contains(std::make_pair(from, endpoint.address))) {
      state->result = Err(rpc::RpcErrorType::NetworkError);
      state->event.Signal();
      return;
    }

    if (is_network_error) {
      state->result = Err(rpc::RpcErrorType::NetworkError);
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

void World::CloseLink(const Address& from, const Address& to) noexcept {
  closed_links_.emplace(std::make_pair(from, to));
}

void World::RestoreLink(const Address& from, const Address& to) noexcept {
  closed_links_.erase(std::make_pair(from, to));
}

void World::FlushAllLogs() noexcept {
  for (auto& [_, host] : hosts_) {
    host->GetLogger()->flush();
  }
}

World* GetWorld() noexcept {
  static World world;
  return &world;
}

}  // namespace ceq::rt::sim
