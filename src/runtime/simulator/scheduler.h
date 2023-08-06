#pragma once

#include "host.h"

#include <boost/fiber/all.hpp>

#include <random>

namespace ceq::rt::sim {

class RuntimeSimulationProps : public boost::fibers::fiber_properties {
 public:
  explicit RuntimeSimulationProps(boost::fibers::context* ctx) noexcept;
  void MarkAsMainFiber();
  bool IsMainFiber() const;

  bool HostIsInitialized() const noexcept;
  Host* GetCurrentHost() const noexcept;
  void SetCurrentHost(Host* host, size_t epoch) noexcept;

  size_t GetCurrentEpoch() const noexcept;

 private:
  bool is_main_ = false;

  bool host_initialized_ = false;
  Host* current_host_ = nullptr;
  size_t host_epoch_ = 0;
};

class RuntimeSimulationScheduler final
    : public boost::fibers::algo::algorithm_with_properties<RuntimeSimulationProps> {
 public:
  explicit RuntimeSimulationScheduler(std::mt19937& generator) noexcept;

  void awakened(boost::fibers::context* ctx, RuntimeSimulationProps& props) noexcept override;

  boost::fibers::context* pick_next() noexcept override;

  bool has_ready_fibers() const noexcept override;

  void property_change(boost::fibers::context* ctx,
                       RuntimeSimulationProps& props) noexcept override;

  void suspend_until(std::chrono::steady_clock::time_point const& time_point) noexcept override;

  void notify() noexcept override;

 private:
  std::vector<boost::fibers::context*> ready_fibers_;
  boost::fibers::context* main_fiber_ = nullptr;
  Host* last_host_ = nullptr;
  size_t last_epoch_ = 0;

  std::mt19937& generator_;
};

}  // namespace ceq::rt::sim
