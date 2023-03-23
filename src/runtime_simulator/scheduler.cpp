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
  if (rqueue_.empty()) {
    return nullptr;
  }
  boost::fibers::context* ctx(&rqueue_.front());
  rqueue_.pop_front();
  last_host_ = properties(ctx).GetCurrentHost();
  return ctx;
}

bool RuntimeSimulationScheduler::has_ready_fibers() const noexcept {
  return !rqueue_.empty();
}

void RuntimeSimulationScheduler::property_change(boost::fibers::context* ctx,
                                                 RuntimeSimulationProps& props) noexcept {
  if (!ctx->ready_is_linked()) {
    return;
  }
  ctx->ready_unlink();
  awakened(ctx, props);
}

void RuntimeSimulationScheduler::suspend_until(
    std::chrono::steady_clock::time_point const& time_point) noexcept {
  if ((std::chrono::steady_clock::time_point::max)() == time_point) {
    std::unique_lock guard(mtx_);
    cnd_.wait(guard, [this]() {
      return flag_;
    });
    flag_ = false;
  } else {
    std::unique_lock<std::mutex> guard(mtx_);
    cnd_.wait_until(guard, time_point, [this]() {
      return flag_;
    });
    flag_ = false;
  }
}

void RuntimeSimulationScheduler::notify() noexcept {
  std::unique_lock<std::mutex> lk(mtx_);
  flag_ = true;
  lk.unlock();
  cnd_.notify_all();
}

}  // namespace runtime_simulation
