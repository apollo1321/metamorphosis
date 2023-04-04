#include "host.h"
#include "logger.h"
#include "scheduler.h"
#include "world.h"

#include <util/binary_search.h>
#include <util/condition_check.h>

namespace ceq::rt {

Host::Host(const Address& address, IHostRunnable* host_main, const HostOptions& options) noexcept
    : logger_{CreateLogger(address)} {
  std::uniform_real_distribution<double> drift_dist{options.drift_interval.first,
                                                    options.drift_interval.second};
  std::uniform_int_distribution<Duration::rep> skew_dist{
      options.start_time_interval.first.count(), options.start_time_interval.second.count()};

  drift_ = drift_dist(GetGenerator());
  start_time_ = Timestamp(static_cast<Duration>(skew_dist(GetGenerator())));
  max_sleep_lag_ = options.max_sleep_lag;

  main_fiber_ = boost::fibers::fiber{boost::fibers::launch::dispatch, [this, host_main]() {
                                       RunMain(host_main);
                                     }};
}

Timestamp Host::GetLocalTime() const noexcept {
  return ToLocalTime(GetWorld()->GetGlobalTime());
}

bool Host::SleepUntil(Timestamp local_time, StopToken stop_token) noexcept {
  VERIFY(GetCurrentHost() == this, "invalid current_host");

  std::uniform_int_distribution<Duration::rep> lag_dist{0, max_sleep_lag_.count()};
  auto val = lag_dist(GetGenerator());
  local_time += Duration(val);

  auto global_timestamp = ToGlobalTime(local_time);
  VERIFY(global_timestamp >= GetWorld()->GetGlobalTime(), "invalid timestamp for sleep");

  GetWorld()->SleepUntil(global_timestamp, stop_token);

  VERIFY(GetWorld()->GetGlobalTime() == global_timestamp || stop_token.StopRequested(),
         "SleepUntil error");

  return stop_token.StopRequested();
}

void Host::RunMain(IHostRunnable* host_main) noexcept {
  boost::this_fiber::properties<RuntimeSimulationProps>().SetCurrentHost(this);
  SleepUntil(start_time_, StopToken{});
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
  VERIFY(GetCurrentHost() == this, "invalid current_host");

  if (!servers_.contains(port)) {
    return Err(RpcError::ErrorType::ConnectionRefused);
  }

  return servers_[port]->ProcessRequest(data, service_name, handler_name);
}

std::shared_ptr<spdlog::logger> Host::GetLogger() noexcept {
  return logger_;
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
