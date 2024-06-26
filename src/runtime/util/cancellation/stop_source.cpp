#include "stop_source.h"

#include <mutex>

namespace mtf::rt {

StopSource::StopSource() noexcept : state_{new impl::StopState{}} {
  state_->Ref();
}

StopSource::StopSource(StopSource&& other) noexcept {
  std::swap(other.state_, state_);
}

StopSource& StopSource::operator=(StopSource&& other) noexcept {
  std::swap(other.state_, state_);
  return *this;
}

StopToken StopSource::GetToken() noexcept {
  return StopToken(state_);
}

void StopSource::Stop() noexcept {
  if (state_ == nullptr) {
    return;
  }

  std::lock_guard guard(state_->lock);
  state_->cancelled = true;
  while (state_->callbacks != nullptr) {
    state_->callbacks->Execute();
  }
}

bool StopSource::StopRequested() const noexcept {
  std::lock_guard guard(state_->lock);
  return state_->cancelled;
}

StopSource::~StopSource() {
  if (state_ != nullptr) {
    state_->UnRef();
  }
}

}  // namespace mtf::rt
