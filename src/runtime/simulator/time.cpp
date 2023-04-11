#include <runtime/time.h>

#include "host.h"

namespace ceq::rt {

Timestamp Now() noexcept {
  return sim::GetCurrentHost()->GetLocalTime();
}

bool SleepFor(Duration duration, StopToken stop_token) noexcept {
  return SleepUntil(Now() + duration, std::move(stop_token));
}

bool SleepUntil(Timestamp timestamp, StopToken stop_token) noexcept {
  return sim::GetCurrentHost()->SleepUntil(timestamp, std::move(stop_token));
}

}  // namespace ceq::rt
