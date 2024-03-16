#include "latch.h"

#include <mutex>

namespace mtf::rt {

LatchGuard Latch::MakeGuard() noexcept {
  return LatchGuard(this);
}

void Latch::AwaitZero() noexcept {
  std::unique_lock guard(mutex_);
  condvar_.wait(guard, [this]() {
    return count_ == 0;
  });
}

LatchGuard::LatchGuard(LatchGuard&& other) noexcept : LatchGuard{} {
  *this = std::move(other);
}

LatchGuard& LatchGuard::operator=(LatchGuard&& other) noexcept {
  std::swap(latch_, other.latch_);
  return *this;
}

LatchGuard::LatchGuard(Latch* latch) noexcept : latch_(latch) {
  std::lock_guard guard(latch_->mutex_);
  ++latch_->count_;
}

LatchGuard::~LatchGuard() {
  if (latch_ == nullptr) {
    return;
  }
  std::lock_guard guard(latch_->mutex_);
  if (--latch_->count_ == 0) {
    latch_->condvar_.notify_all();
  }
}

}  // namespace mtf::rt
