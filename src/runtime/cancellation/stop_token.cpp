#include "stop_token.h"

namespace ceq::rt {

StopToken::StopToken(const StopToken& other) noexcept : StopToken(other.state_) {
}

StopToken::StopToken(StopToken&& other) noexcept {
  std::swap(other.state_, state_);
}

StopToken& StopToken::operator=(StopToken other) noexcept {
  std::swap(other.state_, state_);
  return *this;
}

bool StopToken::StopRequested() const noexcept {
  if (state_ == nullptr) {
    return false;
  }
  std::lock_guard guard(state_->lock);
  return state_->cancelled;
}

StopToken::StopToken(impl::StopState* state) noexcept : state_{state} {
  if (state_ != nullptr) {
    state_->Ref();
  }
}

StopToken::~StopToken() {
  if (state_ != nullptr) {
    state_->UnRef();
  }
}

}  // namespace ceq::rt
