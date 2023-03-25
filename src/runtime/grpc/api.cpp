#include <runtime/api.h>

#include <boost/fiber/all.hpp>

#include <runtime/cancellation/stop_callback.h>

namespace ceq::rt {

Timestamp Now() noexcept {
  return std::chrono::time_point_cast<Duration>(std::chrono::steady_clock::now());
}

void SleepFor(Duration duration, StopToken stop_token) noexcept {
  SleepUntil(Now() + duration, std::move(stop_token));
}

void SleepUntil(Timestamp timestamp, StopToken stop_token) noexcept {
  boost::fibers::condition_variable cv;
  boost::fibers::mutex lock;

  bool cancelled = false;

  StopCallback stop_callback(stop_token, [&]() {
    std::lock_guard guard(lock);
    cancelled = true;
    cv.notify_all();
  });

  std::unique_lock guard(lock);
  cv.wait_until(guard, timestamp, [&]() {
    return cancelled;
  });
}

std::mt19937& GetGenerator() noexcept {
  static thread_local std::mt19937 generator(42);
  return generator;
}

}  // namespace ceq::rt
