#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <runtime/simulator/ut/test_service.client.h>
#include <runtime/simulator/ut/test_service.pb.h>
#include <runtime/simulator/ut/test_service.service.h>

using namespace std::chrono_literals;

using namespace ceq::rt;  // NOLINT

TEST(RuntimeSimulatorHostKill, SimplyWorks) {
  struct Supervisor : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);
      KillHost("addr1");
      SleepFor(1h);
      StartHost("addr1");
    }
  };

  struct Host final : public IHostRunnable {
    void Main() noexcept override {
      ++count;
      ceq::rt::SleepFor(10s);
      ++count;
    }

    size_t count = 0;
  };

  Host host;
  Supervisor supervisor;

  InitWorld(42);
  AddHost("addr1", &host);
  AddHost("supervisor", &supervisor);
  RunSimulation();

  EXPECT_EQ(host.count, 3);

  FinishTest();
}
