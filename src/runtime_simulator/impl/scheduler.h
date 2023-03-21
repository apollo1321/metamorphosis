#pragma once

#include <mutex>

#include <boost/fiber/all.hpp>

namespace runtime_simulation {

class RuntimeSimulationProps : public boost::fibers::fiber_properties {
 public:
  explicit RuntimeSimulationProps(boost::fibers::context* ctx) noexcept;
  void MarkAsMainFiber();
  bool IsMainFiber() const;

 private:
  bool is_main_ = false;
};

class RuntimeSimulationScheduler
    : public boost::fibers::algo::algorithm_with_properties<RuntimeSimulationProps> {
 public:
  void awakened(boost::fibers::context* ctx, RuntimeSimulationProps& props) noexcept override;

  boost::fibers::context* pick_next() noexcept override;

  bool has_ready_fibers() const noexcept override;

  void property_change(boost::fibers::context* ctx,
                       RuntimeSimulationProps& props) noexcept override;

  void suspend_until(std::chrono::steady_clock::time_point const& time_point) noexcept override;

  void notify() noexcept override;

 private:
  boost::fibers::scheduler::ready_queue_type rqueue_;
  std::mutex mtx_{};
  std::condition_variable cnd_{};
  bool flag_{false};
};

}  // namespace runtime_simulation
