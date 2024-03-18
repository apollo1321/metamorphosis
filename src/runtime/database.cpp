#include "database.h"

#ifdef SIMULATION
#include "simulator/database.h"
#else
#include "production/database.h"
#endif

namespace mtf::rt::db {

DBError::DBError(DBErrorType error_type, const std::string& message) noexcept
    : error_type{error_type}, status_message{std::move(message)} {
}

std::string DBError::Message() const noexcept {
  std::string result = [&] {
    switch (error_type) {
      case DBErrorType::Internal:
        return "Internal";
      case DBErrorType::NotFound:
        return "NotFound";
      case DBErrorType::InvalidArgument:
        return "InvalidArgument";
      default:
        VERIFY(false, "invalid error type");
    }
  }();
  if (!status_message.empty()) {
    result += ": " + status_message;
  }
  return result;
}

Result<DatabasePtr, DBError> Open(std::filesystem::path path, Options options) noexcept {
#ifdef SIMULATION
  return sim::db::Database::Open(std::move(path), options);
#else
  return prod::db::Database::Open(std::move(path), options);
#endif
}

}  // namespace mtf::rt::db
