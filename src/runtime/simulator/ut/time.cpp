#include <unordered_set>

#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

using namespace std::chrono_literals;
using namespace mtf::rt;  // NOLINT

TEST(SimulatorTime, SimpleOneHost) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      EXPECT_EQ(mtf::rt::Now(), Timestamp(0h));
      mtf::rt::SleepFor(mtf::rt::Duration(24h));
      EXPECT_EQ(mtf::rt::Now(), Timestamp(24h));
      mtf::rt::SleepFor(mtf::rt::Duration(24h));
      EXPECT_EQ(mtf::rt::Now(), Timestamp(48h));
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::RunSimulation();
}

TEST(SimulatorTime, WorldInitialization) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
    }
  };

  Host host;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::RunSimulation();

  EXPECT_DEATH(sim::RunSimulation(), "");  // NOLINT
}

TEST(SimulatorTime, HostOrdering) {
  struct Host1 final : public sim::IHostRunnable {
    explicit Host1(std::vector<int>& ids) : ids{ids} {
    }

    void Main() noexcept override {
      auto start = mtf::rt::Now();
      mtf::rt::SleepUntil(start + 2s);
      ids.push_back(1);
      mtf::rt::SleepUntil(start + 3s);
      ids.push_back(1);
    }

    std::vector<int>& ids;
  };

  struct Host2 final : public sim::IHostRunnable {
    explicit Host2(std::vector<int>& ids) : ids{ids} {
    }

    void Main() noexcept override {
      auto start = mtf::rt::Now();
      mtf::rt::SleepUntil(start + 1s);
      ids.push_back(2);
      mtf::rt::SleepUntil(start + 4s);
      ids.push_back(2);
      mtf::rt::SleepUntil(start + 6s);
      ids.push_back(2);
    }

    std::vector<int>& ids;
  };

  std::vector<int> ids;

  Host1 host1(ids);
  Host2 host2(ids);

  sim::InitWorld(42);
  sim::AddHost("addr1", &host1, sim::HostOptions{.start_time = {10s, 20s}});
  sim::AddHost("addr2", &host2);
  sim::RunSimulation();

  EXPECT_EQ(ids, std::vector<int>({2, 1, 1, 2, 2}));
}

TEST(SimulatorTime, Loop) {
  struct Host final : public sim::IHostRunnable {
    explicit Host(int& prev, Duration start_sleep, int id)
        : prev{prev}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = mtf::rt::Now() + 10h;
      mtf::rt::SleepFor(start_sleep);
      while (mtf::rt::Now() < end_time) {
        mtf::rt::SleepFor(1s);
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

  sim::InitWorld(42);
  sim::AddHost("addr1", &host1);
  sim::AddHost("addr2", &host2);
  sim::RunSimulation();
}

TEST(SimulatorTime, Drift) {
  struct Host final : public sim::IHostRunnable {
    explicit Host(int& prev, int& inconsistency_count, Duration start_sleep, int id)
        : prev{prev}, inconsistency_count{inconsistency_count}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = mtf::rt::Now() + 10h;
      mtf::rt::SleepFor(start_sleep);
      while (mtf::rt::Now() < end_time) {
        mtf::rt::SleepFor(1s);
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

  sim::InitWorld(42);
  sim::AddHost("addr1", &host1);
  sim::AddHost("addr2", &host2, sim::HostOptions{.drift_interval = {0.001, 0.002}});
  sim::RunSimulation();

  EXPECT_GE(inconsistency_count, 5);
}

TEST(SimulatorTime, SleepLag) {
  struct Host final : public sim::IHostRunnable {
    explicit Host(int& prev, int& inconsistency_count, Duration start_sleep, int id)
        : prev{prev}, inconsistency_count{inconsistency_count}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = mtf::rt::Now() + 10h;
      mtf::rt::SleepFor(start_sleep);
      while (mtf::rt::Now() < end_time) {
        mtf::rt::SleepFor(1s);
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

  sim::InitWorld(42);
  sim::AddHost("addr1", &host1);
  sim::AddHost("addr2", &host2, sim::HostOptions{.max_sleep_lag = 1ms});
  sim::RunSimulation();

  EXPECT_GE(inconsistency_count, 5);
}

TEST(SimulatorTime, AwaitFiberInHost) {
  struct Host final : public sim::IHostRunnable {
    Host(Duration first_dur, Duration second_dur, Duration& last)
        : first_dur{first_dur}, second_dur{second_dur}, last{last} {
    }

    void Main() noexcept override {
      {
        boost::fibers::fiber handle(boost::fibers::launch::post, [&]() {
          mtf::rt::SleepFor(first_dur);
        });

        handle.join();
      }

      EXPECT_EQ(mtf::rt::Now().time_since_epoch(), first_dur);

      {
        boost::fibers::fiber handle(boost::fibers::launch::dispatch, [&]() {
          mtf::rt::SleepFor(second_dur);
        });

        handle.join();
      }

      auto current = mtf::rt::Now().time_since_epoch();
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

  sim::InitWorld(42);
  sim::AddHost("addr1", &host1);
  sim::AddHost("addr2", &host2);
  sim::AddHost("addr3", &host3);
  sim::AddHost("addr4", &host4);
  sim::AddHost("addr5", &host5);
  sim::RunSimulation();
}

TEST(SimulatorTime, Dispatch) {
  struct Host final : public sim::IHostRunnable {
    explicit Host(std::unordered_set<uint64_t>& hosts) : hosts{hosts} {
    }

    void Main() noexcept override {
      auto start_time = mtf::rt::Now();

      const auto host_id = sim::GetHostUniqueId();
      EXPECT_FALSE(hosts.contains(host_id));
      hosts.insert(host_id);

      auto h1 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        mtf::rt::SleepFor(1h);
        EXPECT_EQ(host_id, sim::GetHostUniqueId());
        auto h1 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
          EXPECT_EQ(host_id, sim::GetHostUniqueId());
          mtf::rt::SleepFor(1h);
          EXPECT_EQ(host_id, sim::GetHostUniqueId());
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            mtf::rt::SleepFor(1h);
          }).wait();
          mtf::rt::SleepFor(1h);
          EXPECT_EQ(host_id, sim::GetHostUniqueId());
        });

        auto h2 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
          mtf::rt::SleepFor(1h);
          EXPECT_EQ(host_id, sim::GetHostUniqueId());
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            mtf::rt::SleepFor(1h);
          }).wait();
          mtf::rt::SleepFor(1h);
        });
        EXPECT_EQ(host_id, sim::GetHostUniqueId());

        mtf::rt::SleepFor(1h);
        h1.wait();
        mtf::rt::SleepFor(1h);
        h2.wait();
        mtf::rt::SleepFor(1h);
        EXPECT_EQ(host_id, sim::GetHostUniqueId());
      });

      auto h2 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        EXPECT_EQ(host_id, sim::GetHostUniqueId());
        mtf::rt::SleepFor(1h);
        EXPECT_EQ(host_id, sim::GetHostUniqueId());
        auto h1 = boost::fibers::async(boost::fibers::launch::post, [&]() {
          mtf::rt::SleepFor(1h);
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            mtf::rt::SleepFor(1h);
          }).wait();
          EXPECT_EQ(host_id, sim::GetHostUniqueId());
          mtf::rt::SleepFor(1h);
        });

        auto h2 = boost::fibers::async(boost::fibers::launch::post, []() {
          mtf::rt::SleepFor(1h);
          boost::fibers::async(boost::fibers::launch::post, []() {
            mtf::rt::SleepFor(1h);
          }).wait();
          mtf::rt::SleepFor(1h);
        });

        mtf::rt::SleepFor(1h);
        h1.wait();
        mtf::rt::SleepFor(1h);
        h2.wait();
        mtf::rt::SleepFor(1h);
      });

      boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        mtf::rt::SleepFor(1h);
        EXPECT_EQ(host_id, sim::GetHostUniqueId());
        h1.wait();
        mtf::rt::SleepFor(1h);
        EXPECT_EQ(host_id, sim::GetHostUniqueId());
        h2.wait();
        mtf::rt::SleepFor(1h);
        EXPECT_EQ(host_id, sim::GetHostUniqueId());
      }).wait();

      auto duration = mtf::rt::Now() - start_time;
      EXPECT_EQ(duration, 8h);
      EXPECT_EQ(host_id, sim::GetHostUniqueId());
    }

    std::unordered_set<uint64_t>& hosts;
  };

  for (uint64_t seed = 0; seed < 100; ++seed) {
    std::unordered_set<uint64_t> hosts;
    Host host(hosts);

    sim::HostOptions options{
        .start_time = {1h, 2h},
        .drift_interval = {0.001, 0.002},
    };

    sim::InitWorld(42);
    for (size_t i = 0; i < 10; ++i) {
      sim::AddHost("addr" + std::to_string(i), &host, options);
    }
    sim::RunSimulation();
  }
}
