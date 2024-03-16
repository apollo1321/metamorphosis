#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>

using namespace std::chrono_literals;

using mtf::rt::Duration;
using mtf::rt::Timestamp;

TEST(ProductionCancellation, SimplyWorks) {
  auto start = mtf::rt::Now();

  mtf::rt::StopSource source;

  mtf::rt::SleepFor(1s, source.GetToken());

  auto duration = mtf::rt::Now() - start;

  source.Stop();

  EXPECT_LT(duration, 1500ms);
  EXPECT_GE(duration, 1000ms);
}

TEST(ProductionCancellation, CancelSleepSimplyWorks) {
  auto start = mtf::rt::Now();

  mtf::rt::StopSource source;

  auto handle = boost::fibers::async([&]() {
    mtf::rt::SleepFor(100ms);
    source.Stop();
  });

  mtf::rt::SleepFor(1h, source.GetToken());

  auto duration = mtf::rt::Now() - start;

  EXPECT_LT(duration, 1s);
  EXPECT_GE(duration, 100ms);

  handle.wait();
}

TEST(ProductionCancellation, SleepAfterCancel) {
  auto start = mtf::rt::Now();

  mtf::rt::StopSource source;

  source.Stop();

  mtf::rt::SleepFor(1h, source.GetToken());

  auto duration = mtf::rt::Now() - start;

  EXPECT_LT(duration, 1s);
}

TEST(ProductionCancellation, ConcurrentCancel) {
  for (size_t i = 0; i < 100; ++i) {
    auto start = mtf::rt::Now();

    mtf::rt::StopSource source;

    std::vector<std::thread> threads;

    for (size_t thread_id = 0; thread_id < 10; ++thread_id) {
      threads.emplace_back([&, thread_id]() {
        std::vector<boost::fibers::fiber> fibers;

        for (size_t fiber_id = 0; fiber_id < 100; ++fiber_id) {
          fibers.emplace_back([&]() {
            mtf::rt::SleepFor(1h, source.GetToken());
          });
        }

        for (auto& fiber : fibers) {
          fiber.join();
        }
      });
    }

    for (size_t thread_id = 0; thread_id < 4; ++thread_id) {
      threads.emplace_back([&]() {
        source.Stop();
      });
    }

    auto duration = mtf::rt::Now() - start;

    EXPECT_LT(duration, 2s);

    for (auto& thread : threads) {
      thread.join();
    }
  }
}
