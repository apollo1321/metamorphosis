#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

using namespace std::chrono_literals;
using namespace mtf::rt;  // NOLINT

TEST(SimulatorCancellation, CancelSleepSimplyWorks) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      mtf::rt::StopSource source;

      auto handle = boost::fibers::async([&]() {
        mtf::rt::SleepFor(1s);
        source.Stop();
      });

      EXPECT_TRUE(mtf::rt::SleepFor(1h, source.GetToken()));

      EXPECT_EQ(mtf::rt::Now(), Timestamp(1s));

      handle.wait();
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::RunSimulation();
}

TEST(SimulatorCancellation, SleepAfterCancel) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      mtf::rt::StopSource source;

      source.Stop();

      mtf::rt::SleepFor(1h, source.GetToken());

      EXPECT_EQ(mtf::rt::Now(), Timestamp(0s));
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::RunSimulation();
}

TEST(SimulatorCancellation, CancellationDoesNotAffectOther) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      mtf::rt::StopSource source;

      source.Stop();

      auto h1 = boost::fibers::async([&]() {
        mtf::rt::SleepFor(1h, source.GetToken());
      });
      auto h2 = boost::fibers::async([&]() {
        mtf::rt::SleepFor(1h);
      });
      auto h3 = boost::fibers::async([&]() {
        mtf::rt::SleepFor(1h, source.GetToken());
      });

      EXPECT_EQ(mtf::rt::Now(), Timestamp(0s));

      source.Stop();

      EXPECT_EQ(mtf::rt::Now(), Timestamp(0s));

      h1.wait();
      h2.wait();
      h3.wait();

      EXPECT_EQ(mtf::rt::Now(), Timestamp(1h));
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::RunSimulation();
}
