#include "host.h"

#include "world.h"

#include <util/binary_search.h>
#include <util/condition_check.h>

#include "scheduler.h"

namespace ceq::rt {

Host::Host(IHostRunnable* host_main, const HostOptions& options) noexcept {
  std::uniform_real_distribution<double> drift_dist{options.min_drift, options.max_drift};
  std::uniform_int_distribution<Duration::rep> skew_dist{0, options.max_start_time.count()};

  drift_ = drift_dist(GetGenerator());
  start_time_ = Timestamp(static_cast<Duration>(skew_dist(GetGenerator())));
  max_sleep_lag_ = options.max_sleep_lag;

  main_fiber_ = boost::fibers::fiber{boost::fibers::launch::dispatch, [this, host_main]() {
                                       RunMain(host_main);
                                     }};
}

Timestamp Host::GetLocalTime() const noexcept {
  return ToLocalTime(GetWorld()->GlobalTime());
}

void Host::SleepUntil(Timestamp local_time) noexcept {
  VERIFY(GetCurrentHost() == this, "invalid current_host");

  std::uniform_int_distribution<Duration::rep> lag_dist{0, max_sleep_lag_.count()};
  auto val = lag_dist(GetGenerator());
  local_time += Duration(val);

  auto global_timestamp = ToGlobalTime(local_time);
  VERIFY(global_timestamp >= GetWorld()->GlobalTime(), "invalid timestamp for sleep");

  GetWorld()->SleepUntil(global_timestamp);

  VERIFY(GetWorld()->GlobalTime() == global_timestamp, "SleepUntil error");
}

void Host::RunMain(IHostRunnable* host_main) noexcept {
  boost::this_fiber::properties<RuntimeSimulationProps>().SetCurrentHost(this);
  SleepUntil(start_time_);
  host_main->Main();
  GetWorld()->NotifyHostFinish();
}

Timestamp Host::ToLocalTime(Timestamp global_time) const noexcept {
  using namespace std::chrono_literals;

  Duration shift(static_cast<Duration::rep>(drift_ * global_time.time_since_epoch().count()) +
                 start_time_.time_since_epoch().count());

  return global_time + shift;
}

Timestamp Host::ToGlobalTime(Timestamp local_time) const noexcept {
  Timestamp global_time(
      Duration(BinarySearch<Duration::rep>(0, Duration::max().count(), [&](auto val) {
        return ToLocalTime(static_cast<Timestamp>(static_cast<Duration>(val))) >= local_time;
      })));

  VERIFY(ToLocalTime(global_time) >= local_time, "ToGlobalTime error");

  return global_time;
}

void Host::RegisterServer(RpcServer::RpcServerImpl* server, uint16_t port) noexcept {
  VERIFY(!servers_.contains(port), "port is already used");
  servers_[port] = server;
}

void Host::UnregisterServer(uint16_t port) noexcept {
  VERIFY(servers_.contains(port), "server was not registered");
  servers_.erase(port);
}

RpcResult Host::ProcessRequest(uint16_t port, const SerializedData& data,
                               const ServiceName& service_name,
                               const HandlerName& handler_name) noexcept {
  if (!servers_.contains(port)) {
    return RpcResult::Err(RpcError::ErrorType::ConnectionRefused);
  }

  auto result = servers_[port]->ProcessRequest(data, service_name, handler_name);

  return result;
}

Host::~Host() {
  main_fiber_.join();
}

Host* GetCurrentHost() noexcept {
  VERIFY(boost::this_fiber::properties<RuntimeSimulationProps>().HostIsInitialized(),
         "current host is not initialized");
  Host* result = boost::this_fiber::properties<RuntimeSimulationProps>().GetCurrentHost();
  VERIFY(result != nullptr, "system function is called outside server context");
  return result;
}

}  // namespace ceq::rt
