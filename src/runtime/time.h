#pragma once

#include <runtime/util/cancellation/stop_token.h>
#include <runtime/util/chrono/chrono.h>

namespace ceq::rt {

Timestamp Now() noexcept;
bool SleepFor(Duration duration, StopToken stop_token = StopToken{}) noexcept;
bool SleepUntil(Timestamp timestamp, StopToken stop_token = StopToken{}) noexcept;

}  // namespace ceq::rt
