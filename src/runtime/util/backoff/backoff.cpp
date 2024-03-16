#include "backoff.h"

namespace mtf::rt {

Backoff::Backoff(BackoffParams params, std::mt19937& generator) noexcept
    : params_{params}, generator_{generator}, current_{params.initial} {
}

Duration Backoff::Next() noexcept {
  Duration result(std::uniform_int_distribution<Duration::rep>(0, current_.count())(generator_));
  current_ =
      Duration(std::min<Duration::rep>(params_.max.count(), current_.count() * params_.factor));
  return result;
}

}  // namespace mtf::rt
