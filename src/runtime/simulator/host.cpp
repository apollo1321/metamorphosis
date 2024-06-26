#include "host.h"
#include "logger.h"
#include "scheduler.h"
#include "world.h"

#include <runtime/random.h>

#include <util/binary_search.h>
#include <util/condition_check.h>

namespace mtf::rt::sim {

Host::Host(const Address& address, IHostRunnable* host_main, const HostOptions& options) noexcept
    : logger_{CreateLogger(address)}, host_main_{host_main}, address_{address} {
  drift_ = GetRandomFloat(options.drift_interval.first, options.drift_interval.second);
  start_time_ = Timestamp(GetRandomDuration(options.start_time));
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

  StopFiberIfNecessary();

  return stop_token.StopRequested();
}

void Host::RunMain() noexcept {
  if (epoch_ == 0) {
    SleepUntil(start_time_, StopToken{});
  }
  host_main_->Main();
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

void Host::RegisterServer(rpc::Server::ServerImpl* server, uint16_t port) noexcept {
  StopFiberIfNecessary();
  VERIFY(!servers_.contains(port), "port is already used");
  servers_[port] = server;
}

void Host::UnregisterServer(uint16_t port) noexcept {
  StopFiberIfNecessary();
  VERIFY(servers_.contains(port), "server was not registered");
  servers_.erase(port);
}

Result<rpc::SerializedData, rpc::RpcError> Host::ProcessRequest(
    uint16_t port, const rpc::SerializedData& data, const rpc::ServiceName& service_name,
    const rpc::HandlerName& handler_name) noexcept {
  VERIFY(GetCurrentHost() == this, "invalid current_host");
  StopFiberIfNecessary();

  if (!servers_.contains(port)) {
    return Err(rpc::RpcErrorType::ConnectionRefused);
  }

  auto result = servers_[port]->ProcessRequest(data, service_name, handler_name);

  StopFiberIfNecessary();

  return result;
}

Result<rpc::SerializedData, rpc::RpcError> Host::MakeRequest(const Endpoint& endpoint,
                                                             const rpc::SerializedData& data,
                                                             const rpc::ServiceName& service_name,
                                                             const rpc::HandlerName& handler_name,
                                                             StopToken stop_token) noexcept {
  VERIFY(GetCurrentHost() == this, "invalid current_host");
  StopFiberIfNecessary();

  auto result =
      GetWorld()->MakeRequest(address_, endpoint, data, service_name, handler_name, stop_token);

  StopFiberIfNecessary();

  return result;
}

std::shared_ptr<spdlog::logger> Host::GetLogger() noexcept {
  return logger_;
}

void Host::PauseHost() noexcept {
  paused_ = true;
}

void Host::ResumeHost() noexcept {
  paused_ = false;
  pause_cv_.notify_all();
}

void Host::StopFiberIfNecessary() noexcept {
  // Handle host pause
  {
    std::unique_lock guard(pause_lk_);
    pause_cv_.wait(guard, [this]() {
      return !paused_;
    });
  }

  // Handle host kill
  size_t fiber_epoch = sim::GetCurrentEpoch();
  VERIFY(fiber_epoch <= epoch_, "invalid fiber epoch");
  if (fiber_epoch != epoch_) {
    // Block forever
    boost::fibers::mutex lk;
    boost::fibers::condition_variable cv;

    std::unique_lock guard(lk);
    cv.wait(guard, []() {
      return false;  // sleep forever
    });
  }
}

void Host::KillHost() noexcept {
  if (!main_fiber_.joinable()) {
    return;
  }
  servers_.clear();
  main_fiber_.detach();
  ++epoch_;
}

void Host::StartHost() noexcept {
  if (main_fiber_.joinable()) {
    return;
  }
  main_fiber_ = boost::fibers::fiber{[this]() {
    RunMain();
  }};
  main_fiber_.properties<RuntimeSimulationProps>().SetCurrentHost(this, epoch_);
  boost::this_fiber::yield();
}

size_t Host::GetCurrentEpoch() const noexcept {
  return epoch_;
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

}  // namespace mtf::rt::sim
