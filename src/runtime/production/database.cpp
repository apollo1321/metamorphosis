#include "database.h"

#include <util/defer.h>

namespace ceq::rt::db {

using rocksdb::Slice;

Status<DBError> WriteBatch::WriteBatchImpl::Put(DataView key, DataView value) noexcept {
  Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
  Slice value_slice{reinterpret_cast<const char*>(value.data()), value.size()};
  auto status = write_batch_.Put(key_slice, value_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Status<DBError> WriteBatch::WriteBatchImpl::DeleteRange(DataView start_key,
                                                        DataView end_key) noexcept {
  Slice start_key_slice{reinterpret_cast<const char*>(start_key.data()), start_key.size()};
  Slice end_key_slice{reinterpret_cast<const char*>(end_key.data()), end_key.size()};
  auto status = write_batch_.DeleteRange(start_key_slice, end_key_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Status<DBError> WriteBatch::WriteBatchImpl::Delete(DataView key) noexcept {
  Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
  auto status = write_batch_.Delete(key_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

struct Iterator final : public IIterator {
  explicit Iterator(rocksdb::Iterator* impl) : impl{impl} {
  }

  void SeekToLast() noexcept override {
    impl->SeekToLast();
  }

  virtual void SeekToFirst() noexcept override {
    impl->SeekToFirst();
  }

  virtual void Next() noexcept override {
    EnsureIteratorIsValid();
    impl->Next();
  }

  virtual void Prev() noexcept override {
    EnsureIteratorIsValid();
    impl->Prev();
  }

  virtual Data GetKey() noexcept override {
    EnsureIteratorIsValid();
    auto result = impl->key();
    return Data(result.data(), result.data() + result.size());
  }

  virtual Data GetValue() noexcept override {
    EnsureIteratorIsValid();
    auto result = impl->value();
    return Data(result.data(), result.data() + result.size());
  }

  virtual bool Valid() const noexcept override {
    return impl->Valid();
  }

  void EnsureIteratorIsValid() noexcept {
    VERIFY(Valid(), "iterator is invalid");
  }

  std::unique_ptr<rocksdb::Iterator> impl;
};

Result<Database::DatabaseImpl*, DBError> Database::DatabaseImpl::Open(std::filesystem::path path,
                                                                      Options options) noexcept {
  auto result = new Database::DatabaseImpl;
  DEFER {
    delete result;
  };

  result->comparator_.impl = options.comparator;

  rocksdb::Options rocks_options{};
  rocks_options.create_if_missing = options.create_if_missing;
  rocks_options.comparator = &result->comparator_;

  rocksdb::DB* tmp{};
  auto status = rocksdb::DB::Open(rocks_options, path, &tmp);
  if (status.IsInvalidArgument()) {
    return Err(DBErrorType::InvalidArgument, status.ToString());
  }

  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }

  result->database_ = std::unique_ptr<rocksdb::DB>(tmp);
  result->write_options_.sync = true;

  return Ok(std::exchange(result, nullptr));
}

std::unique_ptr<IIterator> Database::DatabaseImpl::NewIterator() noexcept {
  return std::make_unique<Iterator>(database_->NewIterator(read_options_));
}

Status<DBError> Database::DatabaseImpl::Put(DataView key, DataView value) noexcept {
  Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
  Slice value_slice{reinterpret_cast<const char*>(value.data()), value.size()};
  auto status = database_->Put(write_options_, key_slice, value_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Result<Data, DBError> Database::DatabaseImpl::Get(DataView key) noexcept {
  std::string result;
  Slice key_impl{reinterpret_cast<const char*>(key.data()), key.size()};
  auto status = database_->Get(read_options_, key_impl, &result);

  if (status.IsNotFound()) {
    return Err(DBErrorType::NotFound);
  }
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok(Data{result.begin(), result.end()});
}

Status<DBError> Database::DatabaseImpl::DeleteRange(DataView start_key, DataView end_key) noexcept {
  Slice start_key_slice{reinterpret_cast<const char*>(start_key.data()), start_key.size()};
  Slice end_key_slice{reinterpret_cast<const char*>(end_key.data()), end_key.size()};

  auto status = database_->DeleteRange(write_options_, database_->DefaultColumnFamily(),
                                       start_key_slice, end_key_slice);

  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Status<DBError> Database::DatabaseImpl::Delete(DataView key) noexcept {
  Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
  auto status = database_->Delete(write_options_, key_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Status<DBError> Database::DatabaseImpl::Write(class WriteBatch& write_batch) noexcept {
  auto status = database_->Write(write_options_, &write_batch);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

}  // namespace ceq::rt::db
