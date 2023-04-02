#include <spdlog/spdlog.h>

namespace ceq::rt {

std::shared_ptr<spdlog::logger> CreateLogger(std::string host_name) noexcept;

}
