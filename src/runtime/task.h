#pragma once

class FiberTask {
 public:
  virtual void operator()() noexcept = 0;

  virtual ~FiberTask() = default;
};
