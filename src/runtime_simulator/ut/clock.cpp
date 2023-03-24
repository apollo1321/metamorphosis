#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime_simulator/api.h>

using namespace std::chrono_literals;

using runtime_simulation::Duration;
using runtime_simulation::Timestamp;

TEST(Clock, SimpleOneHost) {
  struct Host final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      EXPECT_EQ(runtime_simulation::now(), Timestamp(0h));
      runtime_simulation::sleep_for(runtime_simulation::Duration(24h));
      EXPECT_EQ(runtime_simulation::now(), Timestamp(24h));
      runtime_simulation::sleep_for(runtime_simulation::Duration(24h));
      EXPECT_EQ(runtime_simulation::now(), Timestamp(48h));
    }
  };

  Host host;

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost("addr1", &host);
  runtime_simulation::RunSimulation();
}

TEST(Clock, WorldInitialization) {
  struct Host final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
    }
  };

  Host host;

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost("addr1", &host);
  runtime_simulation::RunSimulation();

  EXPECT_DEATH(runtime_simulation::RunSimulation(), "");  // NOLINT
}

TEST(Clock, HostOrdering) {
  struct Host1 final : public runtime_simulation::IHostRunnable {
    explicit Host1(std::vector<int>& ids) : ids{ids} {
    }

    void Main() noexcept override {
      auto start = runtime_simulation::now();
      runtime_simulation::sleep_until(start + 2s);
      ids.push_back(1);
      runtime_simulation::sleep_until(start + 3s);
      ids.push_back(1);
    }

    std::vector<int>& ids;
  };

  struct Host2 final : public runtime_simulation::IHostRunnable {
    explicit Host2(std::vector<int>& ids) : ids{ids} {
    }

    void Main() noexcept override {
      auto start = runtime_simulation::now();
      runtime_simulation::sleep_until(start + 1s);
      ids.push_back(2);
      runtime_simulation::sleep_until(start + 4s);
      ids.push_back(2);
      runtime_simulation::sleep_until(start + 6s);
      ids.push_back(2);
    }

    std::vector<int>& ids;
  };

  std::vector<int> ids;

  Host1 host1(ids);
  Host2 host2(ids);

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost(
      "addr1", &host1,
      runtime_simulation::HostOptions{.min_start_time = 10s, .max_start_time = 20s});
  runtime_simulation::AddHost("addr2", &host2);
  runtime_simulation::RunSimulation();

  EXPECT_EQ(ids, std::vector<int>({2, 1, 1, 2, 2}));
}

TEST(Clock, Loop) {
  struct Host final : public runtime_simulation::IHostRunnable {
    explicit Host(int& prev, Duration start_sleep, int id)
        : prev{prev}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = runtime_simulation::now() + 10h;
      runtime_simulation::sleep_for(start_sleep);
      while (runtime_simulation::now() < end_time) {
        runtime_simulation::sleep_for(1s);
        EXPECT_TRUE(prev != id);
        if (prev == id) {
          return;
        }
        prev = id;
      }
    }

    int& prev;
    Duration start_sleep;
    int id;
  };

  int prev = 2;

  Host host1(prev, 500ms, 1);
  Host host2(prev, 1000ms, 2);

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost("addr1", &host1);
  runtime_simulation::AddHost("addr2", &host2);
  runtime_simulation::RunSimulation();
}

TEST(Clock, Drift) {
  struct Host final : public runtime_simulation::IHostRunnable {
    explicit Host(int& prev, int& inconsistency_count, Duration start_sleep, int id)
        : prev{prev}, inconsistency_count{inconsistency_count}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = runtime_simulation::now() + 10h;
      runtime_simulation::sleep_for(start_sleep);
      while (runtime_simulation::now() < end_time) {
        runtime_simulation::sleep_for(1s);
        if (prev == id) {
          ++inconsistency_count;
        }
        prev = id;
      }
    }

    int& prev;
    int& inconsistency_count;
    Duration start_sleep;
    int id;
  };

  int prev = 2;
  int inconsistency_count = 0;

  Host host1(prev, inconsistency_count, 500ms, 1);
  Host host2(prev, inconsistency_count, 1000ms, 2);

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost("addr1", &host1);
  runtime_simulation::AddHost(
      "addr2", &host2, runtime_simulation::HostOptions{.min_drift = 0.001, .max_drift = 0.002});
  runtime_simulation::RunSimulation();

  EXPECT_GE(inconsistency_count, 5);
}

TEST(Clock, SleepLag) {
  struct Host final : public runtime_simulation::IHostRunnable {
    explicit Host(int& prev, int& inconsistency_count, Duration start_sleep, int id)
        : prev{prev}, inconsistency_count{inconsistency_count}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = runtime_simulation::now() + 10h;
      runtime_simulation::sleep_for(start_sleep);
      while (runtime_simulation::now() < end_time) {
        runtime_simulation::sleep_for(1s);
        if (prev == id) {
          ++inconsistency_count;
        }
        prev = id;
      }
    }

    int& prev;
    int& inconsistency_count;
    Duration start_sleep;
    int id;
  };

  int prev = 2;
  int inconsistency_count = 0;

  Host host1(prev, inconsistency_count, 500ms, 1);
  Host host2(prev, inconsistency_count, 1000ms, 2);

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost("addr1", &host1);
  runtime_simulation::AddHost("addr2", &host2,
                              runtime_simulation::HostOptions{.max_sleep_lag = 1ms});
  runtime_simulation::RunSimulation();

  EXPECT_GE(inconsistency_count, 5);
}

TEST(Clock, AwaitFiberInHost) {
  struct Host final : public runtime_simulation::IHostRunnable {
    Host(Duration first_dur, Duration second_dur, Duration& last)
        : first_dur{first_dur}, second_dur{second_dur}, last{last} {
    }

    void Main() noexcept override {
      {
        boost::fibers::fiber handle(boost::fibers::launch::post, [&]() {
          runtime_simulation::sleep_for(first_dur);
        });

        handle.join();
      }

      EXPECT_EQ(runtime_simulation::now().time_since_epoch(), first_dur);

      {
        boost::fibers::fiber handle(boost::fibers::launch::dispatch, [&]() {
          runtime_simulation::sleep_for(second_dur);
        });

        handle.join();
      }

      auto current = runtime_simulation::now().time_since_epoch();
      EXPECT_EQ(current, first_dur + second_dur);

      EXPECT_TRUE(last <= current);
      last = current;
    }

    Duration first_dur;
    Duration second_dur;
    Duration& last;
  };

  Duration last = Duration::zero();

  Host host1(5min, 10h, last);
  Host host2(10us, 2ms, last);
  Host host3(100h, 1ms, last);
  Host host4(1s, 1s, last);
  Host host5(1us, 1us, last);

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost("addr1", &host1);
  runtime_simulation::AddHost("addr2", &host2);
  runtime_simulation::AddHost("addr3", &host3);
  runtime_simulation::AddHost("addr4", &host4);
  runtime_simulation::AddHost("addr5", &host5);
  runtime_simulation::RunSimulation();
}

TEST(Clock, Dispatch) {
  struct Host final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      auto start_time = runtime_simulation::now();

      auto h1 = boost::fibers::async(boost::fibers::launch::dispatch, []() {
        runtime_simulation::sleep_for(1h);
        auto h1 = boost::fibers::async(boost::fibers::launch::dispatch, []() {
          runtime_simulation::sleep_for(1h);
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            runtime_simulation::sleep_for(1h);
          }).wait();
          runtime_simulation::sleep_for(1h);
        });

        auto h2 = boost::fibers::async(boost::fibers::launch::dispatch, []() {
          runtime_simulation::sleep_for(1h);
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            runtime_simulation::sleep_for(1h);
          }).wait();
          runtime_simulation::sleep_for(1h);
        });

        runtime_simulation::sleep_for(1h);
        h1.wait();
        runtime_simulation::sleep_for(1h);
        h2.wait();
        runtime_simulation::sleep_for(1h);
      });

      auto h2 = boost::fibers::async(boost::fibers::launch::dispatch, []() {
        runtime_simulation::sleep_for(1h);
        auto h1 = boost::fibers::async(boost::fibers::launch::post, []() {
          runtime_simulation::sleep_for(1h);
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            runtime_simulation::sleep_for(1h);
          }).wait();
          runtime_simulation::sleep_for(1h);
        });

        auto h2 = boost::fibers::async(boost::fibers::launch::post, []() {
          runtime_simulation::sleep_for(1h);
          boost::fibers::async(boost::fibers::launch::post, []() {
            runtime_simulation::sleep_for(1h);
          }).wait();
          runtime_simulation::sleep_for(1h);
        });

        runtime_simulation::sleep_for(1h);
        h1.wait();
        runtime_simulation::sleep_for(1h);
        h2.wait();
        runtime_simulation::sleep_for(1h);
      });

      boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        runtime_simulation::sleep_for(1h);
        h1.wait();
        runtime_simulation::sleep_for(1h);
        h2.wait();
        runtime_simulation::sleep_for(1h);
      }).wait();

      auto duration = runtime_simulation::now() - start_time;
      EXPECT_EQ(duration, 8h);
    }
  };

  Host host;

  runtime_simulation::HostOptions options{
      .min_start_time = 1h,
      .max_start_time = 2h,
      .min_drift = 0.001,
      .max_drift = 0.002,
  };
  for (uint64_t seed = 0; seed < 100; ++seed) {
    runtime_simulation::InitWorld(42);
    for (size_t i = 0; i < 10; ++i) {
      runtime_simulation::AddHost("addr" + std::to_string(i), &host, options);
    }
    runtime_simulation::RunSimulation();
  }
}
