#pragma once

#include <mutex>

#include "stop_token.h"

namespace ceq::rt {

namespace impl {

struct StopCallbackNode {
  StopCallbackNode() = default;
  StopCallbackNode(StopCallbackNode& other) = delete;
  StopCallbackNode& operator=(StopCallbackNode& other) = delete;

  virtual void Execute() noexcept = 0;

  StopCallbackNode* next = nullptr;
  StopCallbackNode* prev = nullptr;
};

}  // namespace impl

template <class Callback>
class StopCallback : private impl::StopCallbackNode {
 public:
  StopCallback(StopToken token, Callback callback) noexcept
      : token_{std::move(token)}, callback_{std::move(callback)} {
    if (!token_.state_) {
      return;
    }

    std::unique_lock guard(token_.state_->lock);
    if (token_.state_->cancelled) {
      guard.unlock();
      Execute();
      return;
    }

    if (token_.state_->callbacks == nullptr) {
      token_.state_->callbacks = this;
    } else {
      token_.state_->callbacks->prev = this;
      next = token_.state_->callbacks;
      token_.state_->callbacks = this;
    }
  }

  ~StopCallback() {
    if (!token_.state_) {
      return;
    }

    std::unique_lock guard(token_.state_->lock);
    Unlink();
  }

 private:
  void Execute() noexcept override final {
    callback_();
    Unlink();
  }

  void Unlink() noexcept {
    if (token_.state_->callbacks == this) {
      token_.state_->callbacks = next;
    }

    if (prev) {
      prev->next = next;
    }
    if (next) {
      next->prev = prev;
    }

    prev = nullptr;
    next = nullptr;
  }

 private:
  Callback callback_;
  StopToken token_;
};

}  // namespace ceq::rt
