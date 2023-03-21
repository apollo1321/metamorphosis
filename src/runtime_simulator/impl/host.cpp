#include "host.h"

#include "world.h"

#include <util/binary_search.h>
#include <util/condition_check.h>

namespace runtime_simulation {

Host::Host(IHostRunnable* host_main, const Address& address, const HostOptions& options) noexcept
    : address_{address} {
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
  VERIFY(current_host == this, "invalid current_host");

  std::uniform_int_distribution<Duration::rep> lag_dist{0, max_sleep_lag_.count()};
  auto val = lag_dist(GetGenerator());
  local_time += Duration(val);

  auto global_timestamp = ToGlobalTime(local_time);
  VERIFY(global_timestamp >= GetWorld()->GlobalTime(), "invalid timestamp for sleep");

  current_host = nullptr;
  Event event;
  GetWorld()->AddEvent(global_timestamp, event);
  event.Await();
  current_host = this;

  VERIFY(GetWorld()->GlobalTime() == global_timestamp, "SleepUntil error");
}

void Host::RunMain(IHostRunnable* host_main) noexcept {
  current_host = this;
  SleepUntil(start_time_);
  (*host_main)();
  current_host = nullptr;

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

Host::~Host() {
  main_fiber_.join();
}

Host* current_host = nullptr;

}  // namespace runtime_simulation
