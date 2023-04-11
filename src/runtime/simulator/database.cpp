#include "database.h"
#include "host.h"

namespace ceq::rt::db {

// Iterator simply makes copy of all data. Should be enough for tests.
struct Iterator final : public IIterator {
  explicit Iterator(sim::HostDatabase::Map storage) : storage{std::move(storage)} {
    iterator = storage.end();
  }

  void SeekToLast() noexcept override {
    sim::GetCurrentHost()->StopFiberIfNecessary();
    iterator = storage.end();
    if (!storage.empty()) {
      iterator = std::prev(iterator);
    }
  }

  virtual void SeekToFirst() noexcept override {
    sim::GetCurrentHost()->StopFiberIfNecessary();
    iterator = storage.begin();
  }

  virtual void Next() noexcept override {
    sim::GetCurrentHost()->StopFiberIfNecessary();
    EnsureIteratorIsValid();
    iterator = std::next(iterator);
  }

  virtual void Prev() noexcept override {
    sim::GetCurrentHost()->StopFiberIfNecessary();
    EnsureIteratorIsValid();
    iterator = std::prev(iterator);
  }

  virtual db::Data GetKey() noexcept override {
    sim::GetCurrentHost()->StopFiberIfNecessary();
    EnsureIteratorIsValid();
    return iterator->first;
  }

  virtual db::Data GetValue() noexcept override {
    sim::GetCurrentHost()->StopFiberIfNecessary();
    EnsureIteratorIsValid();
    return iterator->second;
  }

  virtual bool Valid() const noexcept override {
    sim::GetCurrentHost()->StopFiberIfNecessary();
    return iterator != storage.end();
  }

  void EnsureIteratorIsValid() noexcept {
    VERIFY(Valid(), "iterator is invalid");
  }

  sim::HostDatabase::Map storage;
  sim::HostDatabase::Map::iterator iterator;
};

Result<Database::DatabaseImpl*, Error> Database::DatabaseImpl::Open(std::filesystem::path path,
                                                                    Options options) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  auto path_str = path.string();
  auto& databases = sim::GetCurrentHost()->databases;
  auto it = databases.find(path_str);

  if (options.create_if_missing == false && it == databases.end()) {
    return Err(Error::ErrorType::InvalidArgument, "database is missing");
  }

  if (it == databases.end()) {
    it = databases.emplace(path_str, options.comparator).first;
    it->second.epoch = sim::GetCurrentEpoch();
  } else {
    VERIFY(it->second.epoch <= sim::GetCurrentEpoch(), "invalid state");

    if (it->second.epoch == sim::GetCurrentEpoch()) {
      return Err(Error::ErrorType::Internal, "database is opened twice");
    }
  }

  if (options.comparator->Name() != it->second.comparator_name) {
    return Err(Error::ErrorType::InvalidArgument,
               "comparator names does not match: " + it->second.comparator_name +
                   " != " + options.comparator->Name());
  }

  auto result = new DatabaseImpl;
  result->db_ = &it->second;
  return Ok(result);
}

std::unique_ptr<IIterator> Database::DatabaseImpl::NewIterator() noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  return std::make_unique<Iterator>(db_->data);
}

Status<Error> Database::DatabaseImpl::Put(DataView key, DataView value) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  db_->data[Data(key.begin(), key.end())] = Data(value.begin(), value.end());
  return Ok();
}

Result<Data, Error> Database::DatabaseImpl::Get(DataView key) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  auto it = db_->data.find(Data(key.begin(), key.end()));
  if (it == db_->data.end()) {
    return Err(Error::ErrorType::NotFound);
  }
  return Ok(it->second);
}

Status<Error> Database::DatabaseImpl::DeleteRange(DataView start_key, DataView end_key) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  auto start_it = db_->data.find(Data(start_key.begin(), start_key.end()));
  if (start_it == db_->data.end()) {
    return Ok();
  }
  auto end_it = db_->data.find(Data(end_key.begin(), end_key.end()));
  db_->data.erase(start_it, end_it);
  return Ok();
}

Status<Error> Database::DatabaseImpl::Delete(DataView key) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  db_->data.erase(Data(key.begin(), key.end()));
  return Ok();
}

}  // namespace ceq::rt::db
