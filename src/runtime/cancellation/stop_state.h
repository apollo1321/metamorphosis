#pragma once

#include <atomic>

#include <util/spin_lock.h>

namespace ceq::rt::impl {

struct StopCallbackNode;

struct StopState {
  void UnRef() noexcept;
  void Ref() noexcept;

  StopCallbackNode* callbacks = nullptr;

  std::atomic_bool cancelled = false;
  SpinLock lock;
  uint64_t ref_count = 0;
};

}  // namespace ceq::rt::impl
