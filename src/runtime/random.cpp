#include "random.h"

namespace ceq::rt {

double GetProbability() noexcept {
  return GetRandomFloat(0., 1.);
}

Duration GetRandomDuration(const Interval& interval) noexcept {
  std::uniform_int_distribution<Duration::rep> dist(interval.from.count(), interval.to.count());
  return Duration(dist(GetGenerator()));
}

double GetRandomFloat(double from, double to) noexcept {
  std::uniform_real_distribution<double> dist(from, to);
  return dist(GetGenerator());
}

}  // namespace ceq::rt
