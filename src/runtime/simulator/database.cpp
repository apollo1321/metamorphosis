#include "database.h"
#include "host.h"

namespace mtf::rt::sim::db {

// Iterator simply makes copy of all data. Should be enough for tests.
struct Iterator final : public IIterator {
  explicit Iterator(HostDatabase::Map storage) : storage{std::move(storage)} {
    iterator = storage.end();
  }

  void SeekToLast() noexcept override {
    rt::sim::GetCurrentHost()->StopFiberIfNecessary();
    iterator = storage.end();
    if (!storage.empty()) {
      iterator = std::prev(iterator);
    }
  }

  virtual void SeekToFirst() noexcept override {
    rt::sim::GetCurrentHost()->StopFiberIfNecessary();
    iterator = storage.begin();
  }

  virtual void Next() noexcept override {
    rt::sim::GetCurrentHost()->StopFiberIfNecessary();
    EnsureIteratorIsValid();
    iterator = std::next(iterator);
  }

  virtual void Prev() noexcept override {
    rt::sim::GetCurrentHost()->StopFiberIfNecessary();
    EnsureIteratorIsValid();
    if (iterator == storage.begin()) {
      iterator = storage.end();
    } else {
      iterator = std::prev(iterator);
    }
  }

  virtual db::Data GetKey() noexcept override {
    rt::sim::GetCurrentHost()->StopFiberIfNecessary();
    EnsureIteratorIsValid();
    return iterator->first;
  }

  virtual db::Data GetValue() noexcept override {
    rt::sim::GetCurrentHost()->StopFiberIfNecessary();
    EnsureIteratorIsValid();
    return iterator->second;
  }

  virtual bool Valid() const noexcept override {
    rt::sim::GetCurrentHost()->StopFiberIfNecessary();
    return iterator != storage.end();
  }

  void EnsureIteratorIsValid() noexcept {
    VERIFY(Valid(), "iterator is invalid");
  }

  HostDatabase::Map storage;
  HostDatabase::Map::iterator iterator;
};

Status<DBError> WriteBatch::Put(DataView key, DataView value) noexcept {
  commands_.emplace_back(PutCmd{Data{key.begin(), key.end()}, Data{value.begin(), value.end()}});
  return Ok();
}

Status<DBError> WriteBatch::DeleteRange(DataView start_key, DataView end_key) noexcept {
  commands_.emplace_back(DeleteRangeCmd{Data{start_key.begin(), start_key.end()},
                                       Data{end_key.begin(), end_key.end()}});
  return Ok();
}

Status<DBError> WriteBatch::Delete(DataView key) noexcept {
  commands_.emplace_back(DeleteCmd{Data{key.begin(), key.end()}});
  return Ok();
}

Database::Database(HostDatabase* database) noexcept : database_{database} {
}

std::unique_ptr<IIterator> Database::NewIterator() noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  return std::make_unique<Iterator>(database_->data);
}

Status<DBError> Database::Put(DataView key, DataView value) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  database_->data[Data(key.begin(), key.end())] = Data(value.begin(), value.end());
  return Ok();
}

Result<Data, DBError> Database::Get(DataView key) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  auto it = database_->data.find(Data(key.begin(), key.end()));
  if (it == database_->data.end()) {
    return Err(DBErrorType::NotFound);
  }
  return Ok(it->second);
}

Status<DBError> Database::DeleteRange(DataView start_key, DataView end_key) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  auto start_it = database_->data.lower_bound(Data(start_key.begin(), start_key.end()));
  if (start_it == database_->data.end()) {
    return Ok();
  }
  auto end_it = database_->data.lower_bound(Data(end_key.begin(), end_key.end()));
  database_->data.erase(start_it, end_it);
  return Ok();
}

Status<DBError> Database::Delete(DataView key) noexcept {
  sim::GetCurrentHost()->StopFiberIfNecessary();
  database_->data.erase(Data(key.begin(), key.end()));
  return Ok();
}

WriteBatchPtr Database::MakeWriteBatch() noexcept {
  return std::make_unique<WriteBatch>();
}

Status<DBError> Database::Write(WriteBatchPtr write_batch) noexcept {
  auto& commands = static_cast<WriteBatch*>(write_batch.get())->commands_;
  for (auto& command : commands) {
    if (std::holds_alternative<WriteBatch::PutCmd>(command)) {
      auto& cmd = std::get<WriteBatch::PutCmd>(command);
      Put(cmd.key, cmd.value).ExpectOk();
    } else if (std::holds_alternative<WriteBatch::DeleteRangeCmd>(command)) {
      auto& cmd = std::get<WriteBatch::DeleteRangeCmd>(command);
      DeleteRange(cmd.start_key, cmd.end_key).ExpectOk();
    } else if (std::holds_alternative<WriteBatch::DeleteCmd>(command)) {
      auto& cmd = std::get<WriteBatch::DeleteCmd>(command);
      Delete(cmd.key).ExpectOk();
    } else {
      VERIFY(false, "unexpected batch command");
    }
  }
  return Ok();
}

Result<DatabasePtr, DBError> Database::Open(std::filesystem::path path, Options options) noexcept {
  rt::sim::GetCurrentHost()->StopFiberIfNecessary();
  auto path_str = path.string();
  auto& databases = rt::sim::GetCurrentHost()->databases;
  auto it = databases.find(path_str);

  if (options.create_if_missing == false && it == databases.end()) {
    return Err(DBErrorType::InvalidArgument, "database is missing");
  }

  if (it == databases.end()) {
    it = databases.emplace(path_str, options.comparator).first;
  } else {
    VERIFY(it->second.epoch <= rt::sim::GetCurrentEpoch(), "invalid state");

    if (it->second.epoch == rt::sim::GetCurrentEpoch()) {
      return Err(DBErrorType::Internal, "database is opened twice");
    }
  }
  it->second.epoch = rt::sim::GetCurrentEpoch();

  if (options.comparator->Name() != it->second.comparator_name) {
    return Err(DBErrorType::InvalidArgument,
               "comparator names does not match: " + it->second.comparator_name +
                   " != " + options.comparator->Name());
  }

  return Ok(DatabasePtr(new Database{&it->second}));
}

}  // namespace mtf::rt::sim::db
