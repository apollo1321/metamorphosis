#pragma once

#include "database.h"

#include <runtime/util/serde/serde.h>

#include <functional>
#include <optional>
#include <utility>

namespace mtf::rt::kv {

using db::DBError;
using db::DBErrorType;

using serde::CSerde;

template <CSerde KeySerde, CSerde ValueSerde, class Cmp>
class KVStorage;

template <CSerde KeySerde, CSerde ValueSerde, class Cmp = std::less<>>
using KVStoragePtr = std::unique_ptr<KVStorage<KeySerde, ValueSerde, Cmp>>;

template <CSerde KeySerde, CSerde ValueSerde, class Cmp = std::less<>>
Result<KVStoragePtr<KeySerde, ValueSerde, Cmp>, DBError> Open(std::filesystem::path path,
                                                              db::Options options,
                                                              KeySerde key_serde,
                                                              ValueSerde value_serde,
                                                              Cmp cmp = Cmp{}) noexcept;

// Wrapper over Database
template <CSerde KeySerde, CSerde ValueSerde, class Cmp = std::less<>>
class KVStorage {
  class Iterator;
  class WriteBatch;

 public:
  using Key = decltype(std::declval<KeySerde>().Deserialize(db::DataView()));
  using Value = decltype(std::declval<ValueSerde>().Deserialize(db::DataView()));

 public:
  KVStorage(const KVStorage& other) noexcept = delete;
  KVStorage& operator=(const KVStorage& other) noexcept = delete;

  Iterator NewIterator() noexcept {
    return Iterator(this, db_->NewIterator());
  }

  Status<DBError> Put(const Key& key, const Value& value) noexcept {
    auto key_ser = key_serde_.Serialize(key);
    auto value_ser = value_serde_.Serialize(value);
    db::DataView key_view(key_ser.data(), key_ser.size());
    db::DataView value_view(value_ser.data(), value_ser.size());
    return db_->Put(key_view, value_view);
  }

  Result<Value, DBError> Get(const Key& key) noexcept {
    auto key_ser = key_serde_.Serialize(key);
    db::DataView key_view(key_ser.data(), key_ser.size());
    return db_->Get(key_view).Transform([&](db::Data&& data) {
      return value_serde_.Deserialize(data);
    });
  }

  Status<DBError> DeleteRange(const Key& start_key, const Key& end_key) noexcept {
    auto start_key_ser = key_serde_.Serialize(start_key);
    auto end_key_ser = key_serde_.Serialize(end_key);
    db::DataView start_key_view(start_key_ser.data(), start_key_ser.size());
    db::DataView end_key_view(end_key_ser.data(), end_key_ser.size());
    return db_->DeleteRange(start_key_view, end_key_view);
  }

  Status<DBError> Delete(const Key& key) noexcept {
    auto key_ser = key_serde_.Serialize(key);
    db::DataView key_view(key_ser.data(), key_ser.size());
    return db_->Delete(key_view);
  }

  WriteBatch MakeWriteBatch() noexcept {
    VERIFY(db_, "");
    return WriteBatch(db_->MakeWriteBatch(), this);
  }

  Status<DBError> Write(WriteBatch write_batch) noexcept {
    return db_->Write(std::move(write_batch.write_batch_));
  }

 private:
  struct Comparator : public db::IComparator {
    explicit Comparator(KVStorage* storage, Cmp cmp) noexcept
        : storage{storage}, cmp{std::move(cmp)} {
    }

    int Compare(db::DataView a, db::DataView b) const noexcept override {
      auto a_des = storage->key_serde_.Deserialize(a);
      auto b_des = storage->key_serde_.Deserialize(b);

      if (cmp(a_des, b_des)) {
        return -1;
      }
      if (cmp(b_des, a_des)) {
        return 1;
      }
      return 0;
    }

    const char* Name() const noexcept override {
      return typeid(KVStorage).name();
    }

    KVStorage* storage{};
    Cmp cmp;
  };

  class Iterator {
   public:
    explicit Iterator(KVStorage* storage, std::unique_ptr<db::IIterator> impl)
        : storage_{storage}, impl_{std::move(impl)} {
    }

    void SeekToLast() noexcept {
      impl_->SeekToLast();
    }

    void SeekToFirst() noexcept {
      impl_->SeekToFirst();
    }

    void Next() noexcept {
      impl_->Next();
    }

    void Prev() noexcept {
      impl_->Prev();
    }

    Key GetKey() noexcept {
      auto data = impl_->GetKey();
      return storage_->key_serde_.Deserialize(db::DataView(data.data(), data.size()));
    }

    Value GetValue() noexcept {
      auto data = impl_->GetValue();
      return storage_->value_serde_.Deserialize(db::DataView(data.data(), data.size()));
    }

    bool Valid() noexcept {
      return impl_->Valid();
    }

   private:
    KVStorage* storage_{};
    std::unique_ptr<db::IIterator> impl_;
  };

  class WriteBatch {
   public:
    Status<DBError> Put(const Key& key, const Value& value) noexcept {
      auto key_ser = storage_->key_serde_.Serialize(key);
      auto value_ser = storage_->value_serde_.Serialize(value);
      db::DataView key_view(key_ser.data(), key_ser.size());
      db::DataView value_view(value_ser.data(), value_ser.size());
      return write_batch_->Put(key_view, value_view);
    }

    Status<DBError> DeleteRange(const Key& start_key, const Key& end_key) noexcept {
      auto start_key_ser = storage_->key_serde_.Serialize(start_key);
      auto end_key_ser = storage_->key_serde_.Serialize(end_key);
      db::DataView start_key_view(start_key_ser.data(), start_key_ser.size());
      db::DataView end_key_view(end_key_ser.data(), end_key_ser.size());
      return write_batch_->DeleteRange(start_key_view, end_key_view);
    }

    Status<DBError> Delete(const Key& key) noexcept {
      auto key_ser = storage_->key_serde_.Serialize(key);
      db::DataView key_view(key_ser.data(), key_ser.size());
      return write_batch_->Delete(key_view);
    }

   private:
    explicit WriteBatch(db::WriteBatchPtr write_batch, KVStorage* storage)
        : write_batch_{std::move(write_batch)}, storage_{storage} {
    }

   private:
    db::WriteBatchPtr write_batch_;
    KVStorage* storage_{};

    friend class KVStorage;
  };

 private:
  KVStorage(KeySerde key_serde, ValueSerde value_serde, Cmp cmp) noexcept
      : key_serde_{std::move(key_serde)},
        value_serde_{std::move(value_serde)},
        comparator_{this, std::move(cmp)} {
  }

 private:
  db::DatabasePtr db_;
  KeySerde key_serde_;
  ValueSerde value_serde_;
  Comparator comparator_;

  friend Result<KVStoragePtr<KeySerde, ValueSerde, Cmp>, DBError> Open<>(std::filesystem::path path,
                                                                         db::Options options,
                                                                         KeySerde key_serde,
                                                                         ValueSerde value_serde,
                                                                         Cmp cmp) noexcept;
};

template <CSerde KeySerde, CSerde ValueSerde, class Cmp>
Result<KVStoragePtr<KeySerde, ValueSerde, Cmp>, DBError> Open(std::filesystem::path path,
                                                              db::Options options,
                                                              KeySerde key_serde,
                                                              ValueSerde value_serde,
                                                              Cmp cmp) noexcept {
  VERIFY(options.comparator == nullptr, "KVStorage user-defined comparator are not allowed");

  KVStoragePtr<KeySerde, ValueSerde, Cmp> result(new KVStorage<KeySerde, ValueSerde, Cmp>(
      std::move(key_serde), std::move(value_serde), std::move(cmp)));

  options.comparator = &result->comparator_;

  auto db = db::Open(path, options);
  if (db.HasError()) {
    return Err(std::move(db.GetError()));
  }
  result->db_ = std::move(db.GetValue());
  return Ok(std::move(result));
}

}  // namespace mtf::rt::kv
