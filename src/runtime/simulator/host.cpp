#include "host.h"
#include "logger.h"
#include "scheduler.h"
#include "world.h"

#include <util/binary_search.h>
#include <util/condition_check.h>

namespace ceq::rt {

Host::Host(const Address& address, IHostRunnable* host_main, const HostOptions& options) noexcept
    : logger_{CreateLogger(address)}, host_main_{host_main} {
  std::uniform_real_distribution<double> drift_dist{options.drift_interval.first,
                                                    options.drift_interval.second};
  std::uniform_int_distribution<Duration::rep> skew_dist{
      options.start_time_interval.first.count(), options.start_time_interval.second.count()};

  drift_ = drift_dist(GetGenerator());
  start_time_ = Timestamp(static_cast<Duration>(skew_dist(GetGenerator())));
  max_sleep_lag_ = options.max_sleep_lag;

  StartHost();
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

  WaitIfPaused();
  LockForeverIfOldEpoch();

  return stop_token.StopRequested();
}

void Host::RunMain() noexcept {
  boost::this_fiber::properties<RuntimeSimulationProps>().SetCurrentHost(this, epoch_);
  if (epoch_ == 0) {
    SleepUntil(start_time_, StopToken{});
  }
  host_main_->Main();
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
  WaitIfPaused();
  LockForeverIfOldEpoch();

  if (!servers_.contains(port)) {
    return Err(RpcError::ErrorType::ConnectionRefused);
  }

  auto result = servers_[port]->ProcessRequest(data, service_name, handler_name);

  WaitIfPaused();
  LockForeverIfOldEpoch();

  return result;
}

RpcResult Host::MakeRequest(const Endpoint& endpoint, const SerializedData& data,
                            const ServiceName& service_name, const HandlerName& handler_name,
                            StopToken stop_token) noexcept {
  VERIFY(GetCurrentHost() == this, "invalid current_host");
  WaitIfPaused();
  LockForeverIfOldEpoch();

  auto result = GetWorld()->MakeRequest(endpoint, std::move(data), std::move(service_name),
                                        std::move(handler_name), std::move(stop_token));

  WaitIfPaused();
  LockForeverIfOldEpoch();

  return result;
}

std::shared_ptr<spdlog::logger> Host::GetLogger() noexcept {
  return logger_;
}

void Host::PauseHost() noexcept {
  VERIFY(!std::exchange(paused_, true), "host is already paused");
}

void Host::ResumeHost() noexcept {
  paused_ = false;
  pause_cv_.notify_all();
}

void Host::WaitIfPaused() noexcept {
  std::unique_lock guard(pause_lk_);
  pause_cv_.wait(guard, [this]() {
    return !paused_;
  });
}

void Host::KillHost() noexcept {
  VERIFY(main_fiber_.joinable(), "host is already killed");
  main_fiber_.detach();
  ++epoch_;
}

void Host::StartHost() noexcept {
  VERIFY(!main_fiber_.joinable(), "host is not killed to start again");
  main_fiber_ = boost::fibers::fiber{[this]() {
    RunMain();
  }};
}

size_t Host::GetCurrentEpoch() const noexcept {
  return epoch_;
}

void Host::LockForeverIfOldEpoch() const noexcept {
  size_t fiber_epoch = rt::GetCurrentEpoch();
  VERIFY(fiber_epoch <= epoch_, "invalid fiber epoch");
  if (rt::GetCurrentEpoch() != epoch_) {
    boost::fibers::mutex lk;
    boost::fibers::condition_variable cv;

    std::unique_lock guard(lk);
    cv.wait(guard, []() {
      return false;  // sleep forever
    });
  }
}

Host::~Host() {
  if (main_fiber_.joinable()) {
    main_fiber_.join();
  }
}

Host* GetCurrentHost() noexcept {
  VERIFY(boost::this_fiber::properties<RuntimeSimulationProps>().HostIsInitialized(),
         "current host is not initialized");
  Host* result = boost::this_fiber::properties<RuntimeSimulationProps>().GetCurrentHost();
  VERIFY(result != nullptr, "system function is called outside server context");
  return result;
}

size_t GetCurrentEpoch() noexcept {
  VERIFY(boost::this_fiber::properties<RuntimeSimulationProps>().HostIsInitialized(),
         "current host is not initialized");
  return boost::this_fiber::properties<RuntimeSimulationProps>().GetCurrentEpoch();
}

}  // namespace ceq::rt
