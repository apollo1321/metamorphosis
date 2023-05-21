#pragma once

#include <boost/fiber/all.hpp>

#include <cstddef>

namespace ceq::rt {

class LatchGuard;

class Latch {
 public:
  LatchGuard MakeGuard() noexcept;

  void AwaitZero() noexcept;

 private:
  boost::fibers::mutex mutex_;
  boost::fibers::condition_variable condvar_;

  size_t count_{};

  friend class LatchGuard;
};

class LatchGuard {
 public:
  LatchGuard() noexcept = default;
  LatchGuard(LatchGuard&& other) noexcept;
  LatchGuard& operator=(LatchGuard&& other) noexcept;

  ~LatchGuard();

 private:
  explicit LatchGuard(Latch* latch) noexcept;

 private:
  Latch* latch_{};

  friend class Latch;
};

}  // namespace ceq::rt
