#pragma once

#include <boost/fiber/all.hpp>

namespace mtf::rt {

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

}  // namespace mtf::rt
