#include "parse.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

std::istringstream& operator>>(std::istringstream& in, ceq::rt::Endpoint& endpoint) {
  auto str = in.str();
  auto pos = str.find_last_of(':');
  if (pos == str.npos) {
    throw std::invalid_argument("invalid endpoint format");
  }

  std::string port_str(str.begin() + pos + 1, str.end());

  int port = std::stoi(port_str);
  if (port < 0 || port > std::numeric_limits<ceq::rt::Port>::max()) {
    throw std::invalid_argument("invalid port: " + port_str);
  }
  endpoint.port = port;
  endpoint.address = std::string(str.begin(), str.begin() + pos);

  in.ignore(str.size());

  return in;
}

std::istringstream& operator>>(std::istringstream& in, ceq::rt::Duration& duration) {
  using ceq::rt::Duration;

  static const std::unordered_map<std::string, Duration> kSuffix{
      {"us", Duration(1ull)},
      {"ms", Duration(1'000ull)},
      {"s", Duration(1'000'000ull)},
      {"m", Duration(60 * 1'000'000ull)},
      {"h", Duration(60 * 60 * 1'000'000ull)},
  };

  uint64_t value;
  in >> value;

  std::string suffix;
  in >> suffix;

  auto it = kSuffix.find(suffix);
  if (it == kSuffix.end()) {
    throw std::invalid_argument("invalid duration suffix: " + suffix);
  }

  duration = Duration(value * it->second);

  return in;
}

std::istringstream& operator>>(std::istringstream& in, ceq::rt::Interval& interval) {
  using ceq::rt::Duration;
  auto str = in.str();
  auto pos = str.find_first_of('-');
  if (pos == str.npos) {
    throw std::invalid_argument("invalid duration interval format");
  }

  std::string from(str.begin(), str.begin() + pos);
  std::istringstream from_in(from);
  from_in >> interval.from;

  std::string to(str.begin() + pos + 1, str.end());
  std::istringstream to_in(to);
  to_in >> interval.to;

  in.ignore(str.size());

  return in;
}
