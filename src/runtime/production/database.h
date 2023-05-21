#pragma once

#include <rocksdb/db.h>

#include <runtime/database.h>

namespace ceq::rt::prod::db {

using namespace rt::db;  // NOLINT

struct WriteBatch final : public IWriteBatch {
  Status<DBError> Put(DataView key, DataView value) noexcept override;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept override;
  Status<DBError> Delete(DataView key) noexcept override;

  rocksdb::WriteBatch write_batch;
};

struct Database final : public IDatabase {
  std::unique_ptr<IIterator> NewIterator() noexcept override;

  Status<DBError> Put(DataView key, DataView value) noexcept override;
  Result<Data, DBError> Get(DataView key) noexcept override;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept override;
  Status<DBError> Delete(DataView key) noexcept override;

  WriteBatchPtr MakeWriteBatch() noexcept override;
  Status<DBError> Write(WriteBatchPtr write_batch) noexcept override;

  struct Comparator final : public rocksdb::Comparator {
    int Compare(const rocksdb::Slice& a, const rocksdb::Slice& b) const override {
      return impl->Compare(DataView{reinterpret_cast<const uint8_t*>(a.data()), a.size()},
                           DataView{reinterpret_cast<const uint8_t*>(b.data()), b.size()});
    }

    const char* Name() const override {
      return impl->Name();
    }

    void FindShortestSeparator(std::string*, const rocksdb::Slice&) const override {
    }
    void FindShortSuccessor(std::string*) const override {
    }

    IComparator* impl{};
  };

  Comparator comparator;
  std::unique_ptr<rocksdb::DB> database;

  rocksdb::WriteOptions write_options{};
  rocksdb::ReadOptions read_options{};
};

Result<DatabasePtr, DBError> Open(std::filesystem::path path, Options options) noexcept;

}  // namespace ceq::rt::prod::db
