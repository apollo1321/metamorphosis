#pragma once

#include <functional>
#include <optional>
#include <utility>

#include "database.h"

namespace ceq::rt::kv {

template <class T>
concept CDataOrView = std::same_as<db::Data, T> || std::same_as<db::DataView, T>;

template <class T>
concept CSerde =  //
    requires(T serde, db::DataView data) {
      { serde.Deserialize(data) } noexcept;
      { serde.Serialize(serde.Deserialize(data)) } noexcept -> CDataOrView;
    };

template <class T>
struct Serde {
  db::Data Serialize(const T& value) const noexcept;
  T Deserialize(db::DataView data) const noexcept;
};

template <CSerde KeySerde, CSerde ValueSerde, class Cmp>
class KVStorage;

template <CSerde KeySerde, CSerde ValueSerde, class Cmp = std::less<>>
Result<KVStorage<KeySerde, ValueSerde, Cmp>, db::Error> Open(std::filesystem::path path,
                                                             db::Options options,
                                                             KeySerde key_serde,
                                                             ValueSerde value_serde,
                                                             Cmp cmp = Cmp{}) noexcept;

// Wrapper over Database
template <CSerde KeySerde, CSerde ValueSerde, class Cmp = std::less<>>
class KVStorage {
  class Iterator;

 public:
  using Key = decltype(std::declval<KeySerde>().Deserialize(db::DataView()));
  using Value = decltype(std::declval<ValueSerde>().Deserialize(db::DataView()));

 public:
  KVStorage(KVStorage&& other) noexcept = default;
  KVStorage& operator=(KVStorage&& other) noexcept = default;

  Iterator NewIterator() noexcept {
    return Iterator(impl_.get(), db_->NewIterator());
  }

  Status<db::Error> Put(const Key& key, const Value& value) noexcept {
    auto key_ser = impl_->key_serde.Serialize(key);
    auto value_ser = impl_->value_serde.Serialize(value);
    db::DataView key_view(key_ser.data(), key_ser.size());
    db::DataView value_view(value_ser.data(), value_ser.size());
    return db_->Put(key_view, value_view);
  }

  Result<Value, db::Error> Get(const Key& key) noexcept {
    auto key_ser = impl_->key_serde.Serialize(key);
    db::DataView key_view(key_ser.data(), key_ser.size());
    auto result = db_->Get(key_view);
    if (result.HasError()) {
      return Err(std::move(result.GetError()));
    }
    return Ok(impl_->value_serde.Deserialize(result.GetValue()));
  }

  Status<db::Error> DeleteRange(const Key& start_key, const Key& end_key) noexcept {
    auto start_key_ser = impl_->key_serde.Serialize(start_key);
    auto end_key_ser = impl_->key_serde.Serialize(end_key);
    db::DataView start_key_view(start_key_ser.data(), start_key_ser.size());
    db::DataView end_key_view(end_key_ser.data(), end_key_ser.size());
    return db_->DeleteRange(start_key_view, end_key_view);
  }

  Status<db::Error> Delete(const Key& key) noexcept {
    auto key_ser = impl_->key_serde.Serialize(key);
    db::DataView key_view(key_ser.data(), key_ser.size());
    return db_->Delete(key_view);
  }

 private:
  struct KVStorageImpl : public db::IComparator {
    KVStorageImpl(KeySerde key_serde, ValueSerde value_serde, Cmp cmp)
        : key_serde{std::move(key_serde)},
          value_serde{std::move(value_serde)},
          cmp{std::move(cmp)} {
    }

    int Compare(db::DataView a, db::DataView b) const noexcept override {
      auto a_des = key_serde.Deserialize(a);
      auto b_des = key_serde.Deserialize(b);

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

    KeySerde key_serde;
    ValueSerde value_serde;
    Cmp cmp;
  };

  class Iterator {
   public:
    explicit Iterator(KVStorageImpl* storage, std::unique_ptr<db::IIterator> impl)
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
      return storage_->key_serde.Deserialize(db::DataView(data.data(), data.size()));
    }

    Value GetValue() noexcept {
      auto data = impl_->GetValue();
      return storage_->value_serde.Deserialize(db::DataView(data.data(), data.size()));
    }

    bool Valid() noexcept {
      return impl_->Valid();
    }

   private:
    KVStorageImpl* storage_;
    std::unique_ptr<db::IIterator> impl_;
  };

 private:
  KVStorage(KeySerde key_serde, ValueSerde value_serde, Cmp cmp) noexcept
      : impl_{std::make_unique<KVStorageImpl>(
            KVStorageImpl{std::move(key_serde), std::move(value_serde), std::move(cmp)})} {
  }

 private:
  std::optional<db::Database> db_;
  std::unique_ptr<KVStorageImpl> impl_;

  friend Result<KVStorage<KeySerde, ValueSerde, Cmp>, db::Error> Open<>(std::filesystem::path path,
                                                                        db::Options options,
                                                                        KeySerde key_serde,
                                                                        ValueSerde value_serde,
                                                                        Cmp cmp) noexcept;
};

template <CSerde KeySerde, CSerde ValueSerde, class Cmp>
Result<KVStorage<KeySerde, ValueSerde, Cmp>, db::Error> Open(std::filesystem::path path,
                                                             db::Options options,
                                                             KeySerde key_serde,
                                                             ValueSerde value_serde,
                                                             Cmp cmp) noexcept {
  VERIFY(options.comparator == nullptr, "KVStorage user-defined comparator are not allowed");

  KVStorage result(std::move(key_serde), std::move(value_serde), std::move(cmp));
  options.comparator = result.impl_.get();

  auto db = db::Open(path, options);
  if (db.HasError()) {
    return Err(std::move(db.GetError()));
  }
  result.db_ = std::move(db.GetValue());
  return Ok(std::move(result));
}

}  // namespace ceq::rt::kv
