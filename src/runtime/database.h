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

class WriteBatch {
 public:
  WriteBatch();

  Status<DBError> Put(DataView key, DataView value) noexcept;
  Result<Data, DBError> Get(DataView key) noexcept;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept;
  Status<DBError> Delete(DataView key) noexcept;

  ~WriteBatch();

 private:
  class WriteBatchImpl;

 private:
  WriteBatchImpl* impl_{};
};

// Imitation of the rocksdb API
// https://github.com/facebook/rocksdb
class Database {
 public:
  Database(Database&& other) noexcept;
  Database& operator=(Database&& other) noexcept;

  std::unique_ptr<IIterator> NewIterator() noexcept;

  Status<DBError> Put(DataView key, DataView value) noexcept;
  Result<Data, DBError> Get(DataView key) noexcept;
  Status<DBError> DeleteRange(DataView start_key, DataView end_key) noexcept;
  Status<DBError> Delete(DataView key) noexcept;

  Status<DBError> Write(WriteBatch write_batch) noexcept;

  ~Database();

 private:
  class DatabaseImpl;

 private:
  Database() noexcept = default;

 private:
  DatabaseImpl* impl_{};

  friend Result<Database, DBError> Open(std::filesystem::path path, Options options) noexcept;
  friend class WriteBatch;
};

Result<Database, DBError> Open(std::filesystem::path path, Options options) noexcept;

}  // namespace ceq::rt::db
