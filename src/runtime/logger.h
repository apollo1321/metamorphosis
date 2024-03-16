#pragma once

// clang-format off
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h> // should be included after spdlog.h
// clang-format on

#include <google/protobuf/message.h>

#define LOG(...) SPDLOG_LOGGER_INFO(mtf::rt::GetLogger(), __VA_ARGS__)
#define LOG_DBG(...) SPDLOG_LOGGER_DEBUG(mtf::rt::GetLogger(), __VA_ARGS__)
#define LOG_ERR(...) SPDLOG_LOGGER_ERROR(mtf::rt::GetLogger(), __VA_ARGS__)
#define LOG_CRIT(...) SPDLOG_LOGGER_CRITICAL(mtf::rt::GetLogger(), __VA_ARGS__)

namespace mtf::rt {

std::shared_ptr<spdlog::logger> GetLogger() noexcept;

}

// Print proto messages
std::ostream& operator<<(std::ostream& os, const google::protobuf::Message& proto);
