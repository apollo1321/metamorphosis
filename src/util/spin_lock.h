#pragma once

#include <atomic>

namespace ceq {

class SpinLock {
 public:
  void Lock() {
    while (is_locked_.exchange(true, std::memory_order_acquire)) {
      while (is_locked_.load(std::memory_order_relaxed)) {
      }
    }
  }

  void Unlock() {
    is_locked_.store(false, std::memory_order_release);
  }

  void lock() {  // NOLINT
    Lock();
  }

  void unlock() {  // NOLINT
    Unlock();
  }

 private:
  std::atomic<bool> is_locked_{false};
};

}  // namespace ceq
