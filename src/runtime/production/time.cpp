#include <runtime/time.h>

#include <boost/fiber/all.hpp>

#include <runtime/util/cancellation/stop_callback.h>

namespace mtf::rt {

Timestamp Now() noexcept {
  return std::chrono::time_point_cast<Duration>(std::chrono::steady_clock::now());
}

bool SleepFor(Duration duration, StopToken stop_token) noexcept {
  return SleepUntil(Now() + duration, std::move(stop_token));
}

bool SleepUntil(Timestamp timestamp, StopToken stop_token) noexcept {
  boost::fibers::condition_variable cv;
  boost::fibers::mutex lock;

  bool cancelled = false;

  StopCallback stop_callback(stop_token, [&]() {
    std::lock_guard guard(lock);
    cancelled = true;
    cv.notify_all();
  });

  std::unique_lock guard(lock);
  return cv.wait_until(guard, timestamp, [&]() {
    return cancelled;
  });
}

}  // namespace mtf::rt
