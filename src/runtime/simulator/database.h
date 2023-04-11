#pragma once

#include <map>

#include <runtime/database.h>

namespace ceq::rt::sim {

struct HostDatabase {
  struct Compare {
    explicit Compare(db::IComparator* comparator) noexcept : comparator{comparator} {
    }

    bool operator()(const db::Data& left, const db::Data& right) const noexcept {
      return comparator->Compare(left, right) < 0;
    }

    db::IComparator* comparator;
  };

  using Map = std::map<db::Data, db::Data, Compare>;

  explicit HostDatabase(db::IComparator* comparator) : data(Compare(comparator)) {
    comparator_name = comparator->Name();
  }

  std::string comparator_name;
  Map data;

  size_t epoch = 0;
};

}  // namespace ceq::rt::sim

namespace ceq::rt::db {

class Database::DatabaseImpl {
 public:
  static Result<DatabaseImpl*, Error> Open(std::filesystem::path path, Options options) noexcept;

  std::unique_ptr<IIterator> NewIterator() noexcept;

  Status<Error> Put(DataView key, DataView value) noexcept;
  Result<Data, Error> Get(DataView key) noexcept;
  Status<Error> DeleteRange(DataView start_key, DataView end_key) noexcept;
  Status<Error> Delete(DataView key) noexcept;

 private:
  sim::HostDatabase* db_;
};

}  // namespace ceq::rt::db
