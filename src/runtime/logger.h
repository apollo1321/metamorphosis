#pragma once

// clang-format off
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h> // should be included after spdlog.h
// clang-format on

#include <google/protobuf/message.h>

#define LOG(...) SPDLOG_LOGGER_INFO(ceq::rt::GetLogger(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(ceq::rt::GetLogger(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(ceq::rt::GetLogger(), __VA_ARGS__)

namespace ceq::rt {

std::shared_ptr<spdlog::logger> GetLogger() noexcept;

}

// Print proto messages
std::ostream& operator<<(std::ostream& os, const google::protobuf::Message& proto);
