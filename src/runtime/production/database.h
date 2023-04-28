#pragma once

#include <rocksdb/db.h>

#include <runtime/database.h>

namespace ceq::rt::db {

class Database::DatabaseImpl {
 public:
  static Result<DatabaseImpl*, DBError> Open(std::filesystem::path path, Options options) noexcept;

  std::unique_ptr<IIterator> NewIterator() noexcept;

  Status<DBError> Put(DataView key, DataView value) noexcept;
  Result<Data, DBError> Get(DataView key) noexcept;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept;
  Status<DBError> Delete(DataView key) noexcept;

 private:
  DatabaseImpl() = default;

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

 private:
  Comparator comparator_;
  std::unique_ptr<rocksdb::DB> database_;

  rocksdb::WriteOptions write_options_{};
  rocksdb::ReadOptions read_options_{};
};

}  // namespace ceq::rt::db
