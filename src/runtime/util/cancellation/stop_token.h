#pragma once

#include "stop_state.h"

namespace mtf::rt {

/*
 * Unfortunately clang does not yet support C++20 <stop_token> feature and the proposed
 * [https://github.com/josuttis/jthread] implementation still has bug. Therefore, I had to write my
 * own simplified version of cancellation, which in the future can be replaced with <stop_token>
 * from the standard library.
 *
 * For simplicity, not all functionality from the standard library is supported. For example, if the
 * call to StopCallback destructor is made from withtin the invocation of the callback on the same
 * thread then there will be deadlock.
 */
class StopToken {
 public:
  StopToken() noexcept = default;
  StopToken(const StopToken& other) noexcept;
  StopToken(StopToken&& other) noexcept;

  StopToken& operator=(StopToken other) noexcept;

  bool StopRequested() const noexcept;

  ~StopToken();

 private:
  explicit StopToken(impl::StopState* state) noexcept;

 private:
  impl::StopState* state_ = nullptr;

  friend class StopSource;
  template <class T>
  friend class StopCallback;
};

}  // namespace mtf::rt
