#include "stop_state.h"

#include <mutex>

namespace mtf::rt::impl {

void StopState::UnRef() noexcept {
  std::unique_lock guard(lock);
  --ref_count;
  if (ref_count == 0) {
    guard.unlock();
    delete this;
  }
}

void StopState::Ref() noexcept {
  std::lock_guard guard(lock);
  ++ref_count;
}

}  // namespace mtf::rt::impl
