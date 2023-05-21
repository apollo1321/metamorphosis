#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>
#include <runtime/util/serde/string_serde.h>
#include <runtime/util/serde/u64_serde.h>

using namespace std::chrono_literals;
using namespace ceq::rt;  // NOLINT

TEST(SimulatorDatabase, SimplyWorks) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      db::Options options{.create_if_missing = true};
      auto maybe_kv =
          kv::Open("/tmp/testing_simply_works", options, serde::U64Serde{}, serde::U64Serde{});
      if (maybe_kv.HasError()) {
        LOG_CRIT("error while opening db: {}", maybe_kv.GetError().Message());
      }
      auto& kv = maybe_kv.GetValue();

      EXPECT_TRUE(kv->Get(42).HasError());
      EXPECT_EQ(kv->Get(42).GetError().error_type, db::DBErrorType::NotFound);

      kv->Put(42, 24).ExpectOk();
      EXPECT_EQ(kv->Get(42).GetValue(), 24);

      kv->Delete(42).ExpectOk();
      EXPECT_TRUE(kv->Get(42).HasError());
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr", &host);
  sim::RunSimulation();
}

TEST(SimulatorDatabase, MissingDb) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      db::Options options{.create_if_missing = false};
      auto kv = kv::Open("/tmp/testing_missing_db", options, serde::U64Serde{}, serde::U64Serde{});
      EXPECT_EQ(kv.GetError().error_type, db::DBErrorType::InvalidArgument);
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr", &host);
  sim::RunSimulation();
}

TEST(SimulatorDatabase, DeleteRange) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      db::Options options{.create_if_missing = true};
      auto kv = kv::Open("/tmp/testing_delete_range", options, serde::U64Serde{}, serde::U64Serde{})
                    .GetValue();

      kv->Put(42, 24).ExpectOk();
      EXPECT_EQ(kv->Get(42).GetValue(), 24);

      kv->Put(142, 142).ExpectOk();
      kv->Put(43, 43).ExpectOk();

      kv->DeleteRange(42, 43).ExpectOk();
      EXPECT_TRUE(kv->Get(42).HasError());

      EXPECT_EQ(kv->Get(142).GetValue(), 142);
      EXPECT_EQ(kv->Get(43).GetValue(), 43);

      kv->Put(43, 24).ExpectOk();
      kv->DeleteRange(42, 43).ExpectOk();
      EXPECT_EQ(kv->Get(43).GetValue(), 24);
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr", &host);
  sim::RunSimulation();
}

TEST(SimulatorDatabase, Iterator) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      db::Options options{.create_if_missing = true};
      auto kv = kv::Open("/tmp/testing_iterator", options, serde::U64Serde{}, serde::U64Serde{})
                    .GetValue();

      for (uint64_t index = 0; index < 200; ++index) {
        kv->Put(index, index + 200).ExpectOk();
      }

      auto iterator = kv->NewIterator();
      iterator.SeekToFirst();

      // Change db

      for (uint64_t index = 50; index < 100; ++index) {
        kv->Delete(index).ExpectOk();
      }

      // Check that changes does not affect iterator

      for (uint64_t index = 0; index < 200; ++index, iterator.Next()) {
        EXPECT_TRUE(iterator.Valid());
        EXPECT_EQ(iterator.GetKey(), index);
        EXPECT_EQ(iterator.GetValue(), index + 200);
      }
      EXPECT_FALSE(iterator.Valid());

      kv->DeleteRange(0, 200).ExpectOk();
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr", &host);
  sim::RunSimulation();
}

TEST(SimulatorDatabase, OpenDatabaseTwice) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      db::Options options{.create_if_missing = true};
      auto kv1 = kv::Open("/tmp/testing_open_database_twice", options, serde::U64Serde{},
                          serde::U64Serde{})
                     .GetValue();
      auto kv2 = kv::Open("/tmp/testing_open_database_twice", options, serde::U64Serde{},
                          serde::U64Serde{});
      EXPECT_EQ(kv2.GetError().error_type, db::DBErrorType::Internal);
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr", &host);
  sim::RunSimulation();
}

TEST(SimulatorDatabase, StringSerde) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      db::Options options{.create_if_missing = true};
      auto kv =
          kv::Open("/tmp/testing_string_serde", options, serde::StringSerde{}, serde::U64Serde{})
              .GetValue();

      kv->Put("bbb", 2).ExpectOk();
      kv->Put("aaa", 1).ExpectOk();
      kv->Put("ccc", 3).ExpectOk();

      auto iterator = kv->NewIterator();
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
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr", &host);
  sim::RunSimulation();
}

TEST(SimulatorDatabase, SurvivesRestart) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      db::Options options{.create_if_missing = true};
      auto kv =
          kv::Open("/tmp/testing_survives_kill", options, serde::U64Serde{}, serde::StringSerde{})
              .GetValue();

      if (first_run) {
        first_run = false;
        kv->Put(2, "bbb").ExpectOk();
        kv->Put(1, "aaa").ExpectOk();
        kv->Put(3, "ccc").ExpectOk();
        SleepFor(5s);
      } else {
        EXPECT_EQ(kv->Get(1).GetValue(), "aaa");
        EXPECT_EQ(kv->Get(2).GetValue(), "bbb");
        EXPECT_EQ(kv->Get(3).GetValue(), "ccc");
        second_finished = true;
      }
    }

    bool first_run = true;
    bool second_finished = false;
  };

  struct Supervisor final : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);
      sim::KillHost("addr");
      sim::StartHost("addr");
    }
  };

  Host host;
  Supervisor supervisor;

  sim::InitWorld(42);
  sim::AddHost("addr", &host);
  sim::AddHost("supervisor", &supervisor);
  sim::RunSimulation();

  EXPECT_TRUE(host.second_finished);
}

TEST(SimulatorDatabase, WriteBatch) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      db::Options options{.create_if_missing = true};
      auto maybe_kv =
          kv::Open("/tmp/testing_write_batch", options, serde::U64Serde{}, serde::U64Serde{});
      if (maybe_kv.HasError()) {
        LOG_CRIT("error while opening db: {}", maybe_kv.GetError().Message());
      }
      auto& kv = maybe_kv.GetValue();

      kv->Get(42).ExpectFail();
      kv->Get(43).ExpectFail();

      kv->Put(0, 0).ExpectOk();
      kv->Put(1, 1).ExpectOk();
      kv->Put(101, 101).ExpectOk();

      auto write_batch = kv->MakeWriteBatch();
      write_batch.Put(42, 42).ExpectOk();
      write_batch.Put(43, 43).ExpectOk();
      write_batch.DeleteRange(0, 2).ExpectOk();
      write_batch.Delete(101).ExpectOk();

      kv->Write(std::move(write_batch)).ExpectOk();

      EXPECT_EQ(kv->Get(42).GetValue(), 42);
      EXPECT_EQ(kv->Get(43).GetValue(), 43);

      kv->Get(0).ExpectFail();
      kv->Get(1).ExpectFail();
      kv->Get(101).ExpectFail();

      kv->Delete(42).ExpectOk();
      kv->Delete(43).ExpectOk();
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr", &host);
  sim::RunSimulation();
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  _Exit(RUN_ALL_TESTS());
}
