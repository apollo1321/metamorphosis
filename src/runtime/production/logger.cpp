#include <runtime/logger.h>

namespace mtf::rt {

std::shared_ptr<spdlog::logger> GetLogger() noexcept {
  return spdlog::default_logger();
}

}  // namespace mtf::rt
