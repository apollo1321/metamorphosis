#include "scheduler.h"

namespace mtf::rt::sim {

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

Host* RuntimeSimulationProps::GetCurrentHost() const noexcept {
  return current_host_;
}

void RuntimeSimulationProps::SetCurrentHost(Host* host, size_t epoch) noexcept {
  host_initialized_ = true;
  current_host_ = host;
  host_epoch_ = epoch;
}

bool RuntimeSimulationProps::HostIsInitialized() const noexcept {
  return host_initialized_;
}

size_t RuntimeSimulationProps::GetCurrentEpoch() const noexcept {
  return host_epoch_;
}

RuntimeSimulationScheduler::RuntimeSimulationScheduler(std::mt19937& generator) noexcept
    : generator_{generator} {
}

void RuntimeSimulationScheduler::awakened(boost::fibers::context* ctx,
                                          RuntimeSimulationProps& props) noexcept {
  if (!props.HostIsInitialized()) {
    props.SetCurrentHost(last_host_, last_epoch_);
  }
  VERIFY(!ctx->is_context(boost::fibers::type::dispatcher_context) ||
             props.GetCurrentHost() == nullptr,
         "dispatching fiber has non-null current_host");

  if (props.IsMainFiber()) {
    VERIFY(main_fiber_ == nullptr, "Main fiber is already awakened");
    main_fiber_ = ctx;
  } else {
    ready_fibers_.emplace_back(ctx);
  }
}

boost::fibers::context* RuntimeSimulationScheduler::pick_next() noexcept {
  VERIFY(has_ready_fibers(), "unexpected schedule state (maybe some of fibers are not joined)");
  boost::fibers::context* ctx = nullptr;
  if (ready_fibers_.empty()) {
    std::swap(main_fiber_, ctx);
  } else {
    size_t index = std::uniform_int_distribution<size_t>(0u, ready_fibers_.size() - 1)(generator_);
    ctx = ready_fibers_[index];
    ready_fibers_.erase(ready_fibers_.begin() + index);
  }

  auto current_host = properties(ctx).GetCurrentHost();
  if (current_host != nullptr || properties(ctx).IsMainFiber()) {
    last_host_ = current_host;
    last_epoch_ = properties(ctx).GetCurrentEpoch();
  }
  return ctx;
}

bool RuntimeSimulationScheduler::has_ready_fibers() const noexcept {
  return main_fiber_ || !ready_fibers_.empty();
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

}  // namespace mtf::rt::sim
