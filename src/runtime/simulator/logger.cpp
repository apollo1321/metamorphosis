#include "logger.h"
#include "world.h"

#include <string>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace ceq::rt {

template <class GetTime>
struct TimeFlags : public spdlog::custom_flag_formatter {
  explicit TimeFlags(GetTime get_time, size_t width = 2)
      : get_time{std::move(get_time)}, width{width} {
  }

  void format(const spdlog::details::log_msg&, const std::tm&,
              spdlog::memory_buf_t& dest) override {
    auto result = std::to_string(get_time());
    VERIFY(width >= result.size(), "invalid log flag length");
    result = std::string(width - result.size(), '0') + result;
    dest.append(result);
  }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<TimeFlags>(get_time, width);
  }

  GetTime get_time;
  size_t width;
};

spdlog::pattern_formatter::custom_flags MakeFlags() noexcept {
  spdlog::pattern_formatter::custom_flags flags;

  flags['F'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags(
      []() {
        return GetWorld()->GetGlobalTime().time_since_epoch().count() % 1'000;
      },
      3));
  flags['f'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags(
      []() {
        return GetCurrentHost()->GetLocalTime().time_since_epoch().count() % 1'000;
      },
      3));
  flags['E'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags(
      []() {
        return GetWorld()->GetGlobalTime().time_since_epoch().count() / 1'000 % 1'000;
      },
      3));
  flags['e'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags(
      []() {
        return GetCurrentHost()->GetLocalTime().time_since_epoch().count() / 1'000 % 1'000;
      },
      3));
  flags['S'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags([]() {
    return GetWorld()->GetGlobalTime().time_since_epoch().count() / 1'000 / 1'000 % 60;
  }));
  flags['s'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags([]() {
    return GetCurrentHost()->GetLocalTime().time_since_epoch().count() / 1'000 / 1'000 % 60;
  }));
  flags['M'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags([]() {
    return GetWorld()->GetGlobalTime().time_since_epoch().count() / 1'000 / 1'000 / 60 % 60;
  }));
  flags['m'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags([]() {
    return GetCurrentHost()->GetLocalTime().time_since_epoch().count() / 1'000 / 1'000 / 60 % 60;
  }));
  flags['H'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags([]() {
    return GetWorld()->GetGlobalTime().time_since_epoch().count() / 1'000 / 1'000 / 60 / 60 % 24;
  }));
  flags['h'] = std::unique_ptr<spdlog::custom_flag_formatter>(new TimeFlags([]() {
    return GetCurrentHost()->GetLocalTime().time_since_epoch().count() / 1'000 / 1'000 / 60 / 60 %
           24;
  }));

  return flags;
}

std::shared_ptr<spdlog::logger> CreateLogger(std::string host_name) noexcept {
  auto logger = std::make_shared<spdlog::logger>(host_name);

  auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_st>(host_name + ".host.log",
                                                                          1024 * 1024, 2, true);
  file_sink->set_formatter(std::make_unique<spdlog::pattern_formatter>(
      "G:[%H:%M:%S.%E.%F] L:[%h:%m:%s.%e.%f] [%^%L%$] %v", spdlog::pattern_time_type::local,
      spdlog::details::os::default_eol, MakeFlags()));

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
  console_sink->set_formatter(std::make_unique<spdlog::pattern_formatter>(
      "[%n] G:[%H:%M:%S.%E.%F] L:[%h:%m:%s.%e.%f] [%^%L%$] %v", spdlog::pattern_time_type::local,
      spdlog::details::os::default_eol, MakeFlags()));

  logger->sinks().emplace_back(std::move(file_sink));
  logger->sinks().emplace_back(std::move(console_sink));

  return logger;
}

}  // namespace ceq::rt
