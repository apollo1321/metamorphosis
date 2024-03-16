#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

using namespace std::chrono_literals;

using namespace mtf::rt;  // NOLINT

TEST(SimulatorHostKill, SimplyWorks) {
  struct Supervisor : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);
      sim::KillHost("addr1");
      SleepFor(1h);
      sim::StartHost("addr1");
    }
  };

  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      ++count;
      mtf::rt::SleepFor(10s);
      ++count;
    }

    size_t count = 0;
  };

  Host host;
  Supervisor supervisor;

  sim::InitWorld(42);
  AddHost("addr1", &host);
  AddHost("supervisor", &supervisor);
  sim::RunSimulation();

  EXPECT_EQ(host.count, 3);

  sim::InitWorld(42);
  AddHost("addr1", &host);
  AddHost("supervisor", &supervisor);
  sim::RunSimulation();

  EXPECT_EQ(host.count, 6);
}

TEST(SimulatorHostKill, KillWithoutRestarting) {
  struct Supervisor : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);
      sim::KillHost("addr1");
      sim::PauseHost("addr1");
    }
  };

  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      ++count;
      mtf::rt::SleepFor(10s);
      ++count;
    }

    size_t count = 0;
  };

  Host host;
  Supervisor supervisor;

  sim::InitWorld(42);
  AddHost("addr1", &host);
  AddHost("supervisor", &supervisor);
  sim::RunSimulation();

  EXPECT_EQ(host.count, 1);

  sim::InitWorld(42);
  AddHost("addr1", &host);
  AddHost("supervisor", &supervisor);
  sim::RunSimulation();

  EXPECT_EQ(host.count, 2);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  _Exit(RUN_ALL_TESTS());
}
