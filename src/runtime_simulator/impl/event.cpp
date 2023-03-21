#include "event.h"

namespace runtime_simulation {
void Event::Await() noexcept {
  std::unique_lock guard(mutex_);
  condvar_.wait(guard, [&]() {
    return resumed_;
  });
}

void Event::Signal() noexcept {
  std::unique_lock guard(mutex_);
  resumed_ = true;
  condvar_.notify_all();
}

void Event::Reset() noexcept {
  std::unique_lock guard(mutex_);
  resumed_ = false;
}

}  // namespace runtime_simulation
