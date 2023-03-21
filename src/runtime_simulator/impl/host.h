#pragma once

#include <memory>

#include <boost/fiber/fiber.hpp>

#include <runtime_simulator/api.h>

namespace runtime_simulation {

struct Host {
  Host(IHostRunnable* host_main, const Address& address, const HostOptions& options) noexcept;

  Timestamp GetLocalTime() const noexcept;
  void SleepUntil(Timestamp local_time) noexcept;

  ~Host();

 private:
  void RunMain(IHostRunnable* host_main) noexcept;

  Timestamp ToLocalTime(Timestamp global_time) const noexcept;
  Timestamp ToGlobalTime(Timestamp local_time) const noexcept;

 private:
  Timestamp start_time_;
  double drift_;
  Duration max_sleep_lag_;

  boost::fibers::fiber main_fiber_;

  const Address address_;
};

extern Host* current_host;

using HostPtr = std::unique_ptr<Host>;

}  // namespace runtime_simulation
