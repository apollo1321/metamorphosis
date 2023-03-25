#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/cancellation/stop_source.h>
#include <runtime/simulator/api.h>

using namespace std::chrono_literals;

using ceq::rt::Duration;
using ceq::rt::Timestamp;

TEST(Cancellation, CancelSleepSimplyWorks) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::StopSource source;

      auto handle = boost::fibers::async([&]() {
        ceq::rt::SleepFor(1s);
        source.Stop();
      });

      ceq::rt::SleepFor(1h, source.GetToken());

      EXPECT_EQ(ceq::rt::Now(), Timestamp(1s));

      handle.wait();
    }
  };

  Host host;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::RunSimulation();
}

TEST(Cancellation, SleepAfterCancel) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::StopSource source;

      source.Stop();

      ceq::rt::SleepFor(1h, source.GetToken());

      EXPECT_EQ(ceq::rt::Now(), Timestamp(0s));
    }
  };

  Host host;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::RunSimulation();
}

TEST(Cancellation, CancellationDoesNotAffectOther) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::StopSource source;

      source.Stop();

      auto h1 = boost::fibers::async([&]() {
        ceq::rt::SleepFor(1h, source.GetToken());
      });
      auto h2 = boost::fibers::async([&]() {
        ceq::rt::SleepFor(1h);
      });
      auto h3 = boost::fibers::async([&]() {
        ceq::rt::SleepFor(1h, source.GetToken());
      });

      EXPECT_EQ(ceq::rt::Now(), Timestamp(0s));

      source.Stop();

      EXPECT_EQ(ceq::rt::Now(), Timestamp(0s));

      h1.wait();
      h2.wait();
      h3.wait();

      EXPECT_EQ(ceq::rt::Now(), Timestamp(1h));
    }
  };

  Host host;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::RunSimulation();
}
