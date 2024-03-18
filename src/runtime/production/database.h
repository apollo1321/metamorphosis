#pragma once

#include <rocksdb/db.h>

#include <runtime/database.h>

namespace mtf::rt::prod::db {

using namespace rt::db;  // NOLINT

class WriteBatch final : public IWriteBatch {
 public:
  Status<DBError> Put(DataView key, DataView value) noexcept override;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept override;
  Status<DBError> Delete(DataView key) noexcept override;

 private:
  friend class Database;

  rocksdb::WriteBatch write_batch_;
};

class Database final : public IDatabase {
 public:
  std::unique_ptr<IIterator> NewIterator() noexcept override;

  Status<DBError> Put(DataView key, DataView value) noexcept override;
  Result<Data, DBError> Get(DataView key) noexcept override;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept override;
  Status<DBError> Delete(DataView key) noexcept override;

  WriteBatchPtr MakeWriteBatch() noexcept override;
  Status<DBError> Write(WriteBatchPtr write_batch) noexcept override;

 public:
  static Result<DatabasePtr, DBError> Open(std::filesystem::path path, Options options) noexcept;

 private:
  class ComparatorWrapper final : public rocksdb::Comparator {
   public:
    explicit ComparatorWrapper(IComparator* comparator) noexcept : comparator_{comparator} {
    }

    int Compare(const rocksdb::Slice& a, const rocksdb::Slice& b) const noexcept override {
      return comparator_->Compare(DataView{reinterpret_cast<const uint8_t*>(a.data()), a.size()},
                                  DataView{reinterpret_cast<const uint8_t*>(b.data()), b.size()});
    }

    const char* Name() const noexcept override {
      return comparator_->Name();
    }

    void FindShortestSeparator(std::string*, const rocksdb::Slice&) const noexcept override {
    }

    void FindShortSuccessor(std::string*) const noexcept override {
    }

   private:
    IComparator* comparator_{};
  };

 private:
  explicit Database(IComparator* comparator) noexcept;

 private:
  ComparatorWrapper comparator_;
  std::unique_ptr<rocksdb::DB> database_;

  rocksdb::WriteOptions write_options_{};
  rocksdb::ReadOptions read_options_{};
};

}  // namespace mtf::rt::prod::db
