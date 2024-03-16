#pragma once

#include <runtime/logger.h>

namespace mtf::rt::sim {

std::shared_ptr<spdlog::logger> CreateLogger(std::string host_name) noexcept;

}
