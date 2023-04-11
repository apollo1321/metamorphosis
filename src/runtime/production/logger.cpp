#include <runtime/logger.h>

namespace ceq::rt {

std::shared_ptr<spdlog::logger> GetLogger() noexcept {
  return spdlog::default_logger();
}

}  // namespace ceq::rt
