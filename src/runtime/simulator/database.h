#pragma once

#include <runtime/database.h>

#include <map>
#include <variant>

namespace ceq::rt::sim::db {

using namespace rt::db;  // NOLINT

struct HostDatabase {
  explicit HostDatabase(db::IComparator* comparator) noexcept
      : data(Comparator(comparator)), comparator_name{comparator->Name()} {
  }

  struct Comparator {
    explicit Comparator(db::IComparator* comparator) noexcept : comparator{comparator} {
    }

    bool operator()(const db::Data& left, const db::Data& right) const noexcept {
      return comparator->Compare(left, right) < 0;
    }

    db::IComparator* comparator{};
  };

  using Map = std::map<db::Data, db::Data, Comparator>;

  std::string comparator_name;
  Map data;

  size_t epoch = 0;  // only for checks
};

struct WriteBatch final : public IWriteBatch {
  struct PutCmd {
    Data key;
    Data value;
  };

  struct DeleteRangeCmd {
    Data start_key;
    Data end_key;
  };

  struct DeleteCmd {
    Data key;
  };

  Status<DBError> Put(DataView key, DataView value) noexcept override;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept override;
  Status<DBError> Delete(DataView key) noexcept override;

  std::vector<std::variant<PutCmd, DeleteRangeCmd, DeleteCmd>> commands;
};

struct Database final : public IDatabase {
  explicit Database(HostDatabase* database) noexcept;

  std::unique_ptr<IIterator> NewIterator() noexcept override;

  Status<DBError> Put(DataView key, DataView value) noexcept override;
  Result<Data, DBError> Get(DataView key) noexcept override;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept override;
  Status<DBError> Delete(DataView key) noexcept override;

  WriteBatchPtr MakeWriteBatch() noexcept override;
  Status<DBError> Write(WriteBatchPtr write_batch) noexcept override;

  HostDatabase* database{};
};

Result<DatabasePtr, DBError> Open(std::filesystem::path path, Options options) noexcept;

}  // namespace ceq::rt::sim::db
