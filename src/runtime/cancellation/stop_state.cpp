#include "stop_state.h"

namespace ceq::rt::impl {

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

}  // namespace ceq::rt::impl
