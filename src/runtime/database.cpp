#include "database.h"

#ifdef SIMULATION
#include "simulator/database.h"
#else
#include "production/database.h"
#endif

namespace ceq::rt::db {

Error::Error(ErrorType error_type, const std::string& message) noexcept
    : error_type{error_type}, status_message{std::move(message)} {
}

std::string Error::Message() const noexcept {
  std::string result = [&] {
    switch (error_type) {
      case ErrorType::Internal:
        return "Internal";
      case ErrorType::NotFound:
        return "NotFound";
      case ErrorType::InvalidArgument:
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

Database::Database(Database&& other) noexcept {
  *this = std::move(other);
}

Database& Database::operator=(Database&& other) noexcept {
  std::swap(other.impl_, impl_);
  return *this;
}

std::unique_ptr<IIterator> Database::NewIterator() noexcept {
  return impl_->NewIterator();
}

Status<Error> Database::Put(DataView key, DataView value) noexcept {
  return impl_->Put(key, value);
}

Result<Data, Error> Database::Get(DataView key) noexcept {
  return impl_->Get(key);
}

Status<Error> Database::DeleteRange(DataView start_key, DataView end_key) noexcept {
  return impl_->DeleteRange(start_key, end_key);
}

Status<Error> Database::Delete(DataView key) noexcept {
  return impl_->Delete(key);
}

Database::~Database() {
  delete impl_;
}

Result<Database, Error> Open(std::filesystem::path path, Options options) noexcept {
  VERIFY(options.comparator != nullptr, "comparator is nulltpr");
  auto impl = Database::DatabaseImpl::Open(std::move(path), options);
  if (impl.HasError()) {
    return Err(impl.GetError());
  }

  Database result;
  result.impl_ = impl.GetValue();
  return Ok(std::move(result));
}

}  // namespace ceq::rt::db
