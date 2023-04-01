#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/cancellation/stop_source.h>

using namespace std::chrono_literals;

using ceq::rt::Duration;
using ceq::rt::Timestamp;

TEST(Cancellation, SimplyWorks) {
  auto start = ceq::rt::Now();

  ceq::rt::StopSource source;

  ceq::rt::SleepFor(1s, source.GetToken());

  auto duration = ceq::rt::Now() - start;

  source.Stop();

  EXPECT_LT(duration, 1500ms);
  EXPECT_GE(duration, 1000ms);
}

TEST(Cancellation, CancelSleepSimplyWorks) {
  auto start = ceq::rt::Now();

  ceq::rt::StopSource source;

  auto handle = boost::fibers::async([&]() {
    ceq::rt::SleepFor(100ms);
    source.Stop();
  });

  ceq::rt::SleepFor(1h, source.GetToken());

  auto duration = ceq::rt::Now() - start;

  EXPECT_LT(duration, 1s);
  EXPECT_GE(duration, 100ms);

  handle.wait();
}

TEST(Cancellation, SleepAfterCancel) {
  auto start = ceq::rt::Now();

  ceq::rt::StopSource source;

  source.Stop();

  ceq::rt::SleepFor(1h, source.GetToken());

  auto duration = ceq::rt::Now() - start;

  EXPECT_LT(duration, 1s);
}

TEST(Cancellation, ConcurrentCancel) {
  for (size_t i = 0; i < 100; ++i) {
    auto start = ceq::rt::Now();

    ceq::rt::StopSource source;

    std::vector<std::thread> threads;

    for (size_t thread_id = 0; thread_id < 10; ++thread_id) {
      threads.emplace_back([&, thread_id]() {
        std::vector<boost::fibers::fiber> fibers;

        for (size_t fiber_id = 0; fiber_id < 100; ++fiber_id) {
          fibers.emplace_back([&]() {
            ceq::rt::SleepFor(1h, source.GetToken());
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

    auto duration = ceq::rt::Now() - start;

    EXPECT_LT(duration, 2s);

    for (auto& thread : threads) {
      thread.join();
    }
  }
}
