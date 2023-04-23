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

struct Error {
  enum class ErrorType {
    Internal,
    InvalidArgument,
    NotFound,
  };

  explicit Error(ErrorType error_type = ErrorType::Internal,
                 const std::string& message = "") noexcept;

  std::string Message() const noexcept;

  ErrorType error_type;
  std::string status_message;
};

struct Options {
  IComparator* comparator = nullptr;
  bool create_if_missing = true;
};

// Imitation of the rocksdb API
// https://github.com/facebook/rocksdb
class Database {
 public:
  Database(Database&& other) noexcept;
  Database& operator=(Database&& other) noexcept;

  std::unique_ptr<IIterator> NewIterator() noexcept;

  Status<Error> Put(DataView key, DataView value) noexcept;
  Result<Data, Error> Get(DataView key) noexcept;
  Status<Error> DeleteRange(DataView start_key, DataView end_key) noexcept;
  Status<Error> Delete(DataView key) noexcept;

  ~Database();

 private:
  class DatabaseImpl;

 private:
  Database() noexcept = default;

 private:
  DatabaseImpl* impl_{};

  friend Result<Database, Error> Open(std::filesystem::path path, Options options) noexcept;
};

Result<Database, Error> Open(std::filesystem::path path, Options options) noexcept;

}  // namespace ceq::rt::db
