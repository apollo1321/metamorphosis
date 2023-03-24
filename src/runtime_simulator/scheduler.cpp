#include "scheduler.h"

namespace runtime_simulation {

RuntimeSimulationProps::RuntimeSimulationProps(boost::fibers::context* ctx) noexcept
    : fiber_properties(ctx) {
}

void RuntimeSimulationProps::MarkAsMainFiber() {
  if (!is_main_) {
    is_main_ = true;
    notify();
  }
}

bool RuntimeSimulationProps::IsMainFiber() const {
  return is_main_;
}

Host* RuntimeSimulationProps::GetCurrentHost() noexcept {
  return current_host_;
}

void RuntimeSimulationProps::SetCurrentHost(Host* host) noexcept {
  host_initialized_ = true;
  current_host_ = host;
}

bool RuntimeSimulationProps::HostIsInitialized() noexcept {
  return host_initialized_;
}

void RuntimeSimulationScheduler::awakened(boost::fibers::context* ctx,
                                          RuntimeSimulationProps& props) noexcept {
  if (!props.HostIsInitialized()) {
    props.SetCurrentHost(last_host_);
  }

  if (props.IsMainFiber() || rqueue_.empty()) {
    rqueue_.insert(rqueue_.end(), *ctx);
  } else {
    rqueue_.insert(std::prev(rqueue_.end()), *ctx);
  }
}

boost::fibers::context* RuntimeSimulationScheduler::pick_next() noexcept {
  VERIFY(!rqueue_.empty(), "unexpected schedule state");
  boost::fibers::context* ctx(&rqueue_.front());
  rqueue_.pop_front();
  auto current_host = properties(ctx).GetCurrentHost();
  if (current_host != nullptr || properties(ctx).IsMainFiber()) {
    last_host_ = current_host;
  }
  return ctx;
}

bool RuntimeSimulationScheduler::has_ready_fibers() const noexcept {
  return !rqueue_.empty();
}

void RuntimeSimulationScheduler::property_change(boost::fibers::context* ctx,
                                                 RuntimeSimulationProps& props) noexcept {
  ctx->ready_unlink();
  awakened(ctx, props);
}

void RuntimeSimulationScheduler::suspend_until(
    std::chrono::steady_clock::time_point const& time_point) noexcept {
  VERIFY(false, "suspend_until is not expected to be called");
}

void RuntimeSimulationScheduler::notify() noexcept {
  // No-op
}

}  // namespace runtime_simulation
