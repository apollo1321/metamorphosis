#pragma once

#include <memory>
#include <thread>

#include <util/spin_lock.h>

#include "stop_callback.h"
#include "stop_token.h"

namespace ceq::rt {

class StopSource {
 public:
  StopSource() noexcept;
  StopSource(StopSource&& other) noexcept;
  StopSource& operator=(StopSource&& other) noexcept;

  StopToken GetToken() noexcept;

  void Stop() noexcept;

  ~StopSource();

 private:
  impl::StopState* state_ = nullptr;
};

}  // namespace ceq::rt
