#pragma once

#include <runtime/util/serde/serde.h>

#include <util/result.h>

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ceq::rt::db {

using serde::Data;
using serde::DataView;

class IIterator {
 public:
  virtual void SeekToLast() noexcept = 0;
  virtual void SeekToFirst() noexcept = 0;

  virtual void Next() noexcept = 0;
  virtual void Prev() noexcept = 0;

  virtual Data GetKey() noexcept = 0;
  virtual Data GetValue() noexcept = 0;

  virtual bool Valid() const noexcept = 0;

  virtual ~IIterator() = default;
};

class IComparator {
 public:
  /* Three-way comparison. Returns value:
   *   < 0 iff "a" < "b",
   *   == 0 iff "a" == "b",
   *   > 0 iff "a" > "b"
   */
  virtual int Compare(DataView a, DataView b) const noexcept = 0;

  /* Name of comparator.
   * It is used to check whether previous comparators
   * The name of the comparator.  Used to check for comparator
   * mismatches (i.e., a DB created with one comparator is
   * accessed using a different comparator.
   */
  virtual const char* Name() const noexcept = 0;

  virtual ~IComparator() = default;
};

enum class DBErrorType {
  Internal,
  InvalidArgument,
  NotFound,
};

struct DBError {
  explicit DBError(DBErrorType error_type = DBErrorType::Internal,
                   const std::string& message = "") noexcept;

  std::string Message() const noexcept;

  DBErrorType error_type;
  std::string status_message;
};

struct Options {
  IComparator* comparator = nullptr;
  bool create_if_missing = true;
};

class IWriteBatch {
 public:
  virtual Status<DBError> Put(DataView key, DataView value) noexcept = 0;
  virtual Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept = 0;
  virtual Status<DBError> Delete(DataView key) noexcept = 0;

  virtual ~IWriteBatch() = default;
};

using WriteBatchPtr = std::unique_ptr<IWriteBatch>;

// Imitation of the rocksdb API
// https://github.com/facebook/rocksdb
class IDatabase {
 public:
  virtual std::unique_ptr<IIterator> NewIterator() noexcept = 0;

  virtual Status<DBError> Put(DataView key, DataView value) noexcept = 0;
  virtual Result<Data, DBError> Get(DataView key) noexcept = 0;
  virtual Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept = 0;
  virtual Status<DBError> Delete(DataView key) noexcept = 0;

  virtual WriteBatchPtr MakeWriteBatch() noexcept = 0;
  virtual Status<DBError> Write(WriteBatchPtr write_batch) noexcept = 0;

  virtual ~IDatabase() = default;
};

using DatabasePtr = std::unique_ptr<IDatabase>;

Result<DatabasePtr, DBError> Open(std::filesystem::path path, Options options) noexcept;

}  // namespace ceq::rt::db
