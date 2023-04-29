#include <gtest/gtest.h>

#include <runtime/api.h>
#include <runtime/util/serde/string_serde.h>
#include <runtime/util/serde/u64_serde.h>

using namespace ceq::rt;  // NOLINT

TEST(ProductionDatabase, SimplyWorks) {
  db::Options options{.create_if_missing = true};
  auto maybe_kv =
      kv::Open("/tmp/testing_simply_works", options, serde::U64Serde{}, serde::U64Serde{});
  if (maybe_kv.HasError()) {
    LOG_CRITICAL("error while opening db: {}", maybe_kv.GetError().Message());
  }
  auto& kv = maybe_kv.GetValue();

  EXPECT_TRUE(kv.Get(42).HasError());
  EXPECT_EQ(kv.Get(42).GetError().error_type, db::DBErrorType::NotFound);

  kv.Put(42, 24).ExpectOk();
  EXPECT_EQ(kv.Get(42).GetValue(), 24);

  kv.Delete(42).ExpectOk();
  EXPECT_TRUE(kv.Get(42).HasError());
}

TEST(ProductionDatabase, MissingDb) {
  db::Options options{.create_if_missing = false};
  auto kv = kv::Open("/tmp/testing_missing_db", options, serde::U64Serde{}, serde::U64Serde{});
  EXPECT_EQ(kv.GetError().error_type, db::DBErrorType::InvalidArgument);
}

TEST(ProductionDatabase, DeleteRange) {
  db::Options options{.create_if_missing = true};
  auto kv = kv::Open("/tmp/testing_delete_range", options, serde::U64Serde{}, serde::U64Serde{})
                .GetValue();

  kv.Put(42, 24).ExpectOk();
  EXPECT_EQ(kv.Get(42).GetValue(), 24);

  kv.DeleteRange(42, 43).ExpectOk();
  EXPECT_TRUE(kv.Get(42).HasError());

  kv.Put(43, 24).ExpectOk();
  kv.DeleteRange(42, 43).ExpectOk();
  EXPECT_EQ(kv.Get(43).GetValue(), 24);
}

TEST(ProductionDatabase, Iterator) {
  db::Options options{.create_if_missing = true};
  auto kv =
      kv::Open("/tmp/testing_iterator", options, serde::U64Serde{}, serde::U64Serde{}).GetValue();

  for (uint64_t index = 0; index < 200; ++index) {
    kv.Put(index, index + 200).ExpectOk();
  }

  auto iterator = kv.NewIterator();
  iterator.SeekToFirst();

  // Change db

  for (uint64_t index = 50; index < 100; ++index) {
    kv.Delete(index).ExpectOk();
  }

  // Check that changes does not affect iterator

  for (uint64_t index = 0; index < 200; ++index, iterator.Next()) {
    EXPECT_TRUE(iterator.Valid());
    EXPECT_EQ(iterator.GetKey(), index);
    EXPECT_EQ(iterator.GetValue(), index + 200);
  }
  EXPECT_FALSE(iterator.Valid());

  kv.DeleteRange(0, 200).ExpectOk();
}

TEST(ProductionDatabase, OpenDatabaseTwice) {
  db::Options options{.create_if_missing = true};
  auto kv1 =
      kv::Open("/tmp/testing_open_database_twice", options, serde::U64Serde{}, serde::U64Serde{})
          .GetValue();
  auto kv2 =
      kv::Open("/tmp/testing_open_database_twice", options, serde::U64Serde{}, serde::U64Serde{});
  EXPECT_EQ(kv2.GetError().error_type, db::DBErrorType::Internal);
}

TEST(ProductionDatabase, StringSerde) {
  db::Options options{.create_if_missing = true};
  auto kv = kv::Open("/tmp/testing_string_serde", options, serde::StringSerde{}, serde::U64Serde{})
                .GetValue();

  kv.Put("bbb", 2).ExpectOk();
  kv.Put("aaa", 1).ExpectOk();
  kv.Put("ccc", 3).ExpectOk();

  auto iterator = kv.NewIterator();
  iterator.SeekToFirst();

  EXPECT_TRUE(iterator.Valid());
  EXPECT_EQ(iterator.GetKey(), "aaa");
  EXPECT_EQ(iterator.GetValue(), 1);
  iterator.Next();

  EXPECT_TRUE(iterator.Valid());
  EXPECT_EQ(iterator.GetKey(), "bbb");
  EXPECT_EQ(iterator.GetValue(), 2);
  iterator.Next();

  EXPECT_TRUE(iterator.Valid());
  EXPECT_EQ(iterator.GetKey(), "ccc");
  EXPECT_EQ(iterator.GetValue(), 3);
  iterator.Next();

  EXPECT_FALSE(iterator.Valid());
}
