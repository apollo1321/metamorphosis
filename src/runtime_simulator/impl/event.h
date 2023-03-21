#pragma once

#include <boost/fiber/all.hpp>

namespace runtime_simulation {

struct Event {
 public:
  void Await() noexcept;
  void Signal() noexcept;
  void Reset() noexcept;

 private:
  boost::fibers::mutex mutex_;
  boost::fibers::condition_variable condvar_;
  bool resumed_ = false;
};

}  // namespace runtime_simulation
