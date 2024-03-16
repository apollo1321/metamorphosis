#include "database.h"

#include <util/defer.h>

namespace mtf::rt::prod::db {

using rocksdb::Slice;

Status<DBError> WriteBatch::Put(DataView key, DataView value) noexcept {
  Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
  Slice value_slice{reinterpret_cast<const char*>(value.data()), value.size()};
  auto status = write_batch.Put(key_slice, value_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Status<DBError> WriteBatch::DeleteRange(DataView start_key, DataView end_key) noexcept {
  Slice start_key_slice{reinterpret_cast<const char*>(start_key.data()), start_key.size()};
  Slice end_key_slice{reinterpret_cast<const char*>(end_key.data()), end_key.size()};
  auto status = write_batch.DeleteRange(start_key_slice, end_key_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Status<DBError> WriteBatch::Delete(DataView key) noexcept {
  Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
  auto status = write_batch.Delete(key_slice);
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

std::unique_ptr<IIterator> Database::NewIterator() noexcept {
  return std::make_unique<Iterator>(database->NewIterator(read_options));
}

Status<DBError> Database::Put(DataView key, DataView value) noexcept {
  Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
  Slice value_slice{reinterpret_cast<const char*>(value.data()), value.size()};
  auto status = database->Put(write_options, key_slice, value_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Result<Data, DBError> Database::Get(DataView key) noexcept {
  std::string result;
  Slice key_impl{reinterpret_cast<const char*>(key.data()), key.size()};
  auto status = database->Get(read_options, key_impl, &result);

  if (status.IsNotFound()) {
    return Err(DBErrorType::NotFound);
  }
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok(Data{result.begin(), result.end()});
}

Status<DBError> Database::DeleteRange(DataView start_key, DataView end_key) noexcept {
  Slice start_key_slice{reinterpret_cast<const char*>(start_key.data()), start_key.size()};
  Slice end_key_slice{reinterpret_cast<const char*>(end_key.data()), end_key.size()};

  auto status = database->DeleteRange(write_options, database->DefaultColumnFamily(),
                                      start_key_slice, end_key_slice);

  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Status<DBError> Database::Delete(DataView key) noexcept {
  Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
  auto status = database->Delete(write_options, key_slice);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

WriteBatchPtr Database::MakeWriteBatch() noexcept {
  return std::make_unique<WriteBatch>();
}

Status<DBError> Database::Write(WriteBatchPtr write_batch) noexcept {
  WriteBatch* batch = static_cast<WriteBatch*>(write_batch.get());
  auto status = database->Write(write_options, &batch->write_batch);
  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }
  return Ok();
}

Result<DatabasePtr, DBError> Open(std::filesystem::path path, Options options) noexcept {
  VERIFY(options.comparator != nullptr, "comparator is nulltpr");
  auto result = std::make_unique<Database>();

  result->comparator.impl = options.comparator;

  rocksdb::Options rocks_options{};
  rocks_options.create_if_missing = options.create_if_missing;
  rocks_options.comparator = &result->comparator;

  rocksdb::DB* tmp{};
  auto status = rocksdb::DB::Open(rocks_options, path, &tmp);
  if (status.IsInvalidArgument()) {
    return Err(DBErrorType::InvalidArgument, status.ToString());
  }

  if (!status.ok()) {
    return Err(DBErrorType::Internal, status.ToString());
  }

  result->database = std::unique_ptr<rocksdb::DB>(tmp);
  result->write_options.sync = true;

  return Ok(DatabasePtr(result.release()));
}

}  // namespace mtf::rt::prod::db
