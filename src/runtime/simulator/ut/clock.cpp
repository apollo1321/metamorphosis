#include <unordered_set>

#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

using namespace std::chrono_literals;

using ceq::rt::Duration;
using ceq::rt::Timestamp;

TEST(Clock, SimpleOneHost) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      EXPECT_EQ(ceq::rt::Now(), Timestamp(0h));
      ceq::rt::SleepFor(ceq::rt::Duration(24h));
      EXPECT_EQ(ceq::rt::Now(), Timestamp(24h));
      ceq::rt::SleepFor(ceq::rt::Duration(24h));
      EXPECT_EQ(ceq::rt::Now(), Timestamp(48h));
    }
  };

  Host host;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::RunSimulation();
}

TEST(Clock, WorldInitialization) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
    }
  };

  Host host;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::RunSimulation();

  EXPECT_DEATH(ceq::rt::RunSimulation(), "");  // NOLINT
}

TEST(Clock, HostOrdering) {
  struct Host1 final : public ceq::rt::IHostRunnable {
    explicit Host1(std::vector<int>& ids) : ids{ids} {
    }

    void Main() noexcept override {
      auto start = ceq::rt::Now();
      ceq::rt::SleepUntil(start + 2s);
      ids.push_back(1);
      ceq::rt::SleepUntil(start + 3s);
      ids.push_back(1);
    }

    std::vector<int>& ids;
  };

  struct Host2 final : public ceq::rt::IHostRunnable {
    explicit Host2(std::vector<int>& ids) : ids{ids} {
    }

    void Main() noexcept override {
      auto start = ceq::rt::Now();
      ceq::rt::SleepUntil(start + 1s);
      ids.push_back(2);
      ceq::rt::SleepUntil(start + 4s);
      ids.push_back(2);
      ceq::rt::SleepUntil(start + 6s);
      ids.push_back(2);
    }

    std::vector<int>& ids;
  };

  std::vector<int> ids;

  Host1 host1(ids);
  Host2 host2(ids);

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host1, ceq::rt::HostOptions{.start_time_interval = {10s, 20s}});
  ceq::rt::AddHost("addr2", &host2);
  ceq::rt::RunSimulation();

  EXPECT_EQ(ids, std::vector<int>({2, 1, 1, 2, 2}));
}

TEST(Clock, Loop) {
  struct Host final : public ceq::rt::IHostRunnable {
    explicit Host(int& prev, Duration start_sleep, int id)
        : prev{prev}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = ceq::rt::Now() + 10h;
      ceq::rt::SleepFor(start_sleep);
      while (ceq::rt::Now() < end_time) {
        ceq::rt::SleepFor(1s);
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

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host1);
  ceq::rt::AddHost("addr2", &host2);
  ceq::rt::RunSimulation();
}

TEST(Clock, Drift) {
  struct Host final : public ceq::rt::IHostRunnable {
    explicit Host(int& prev, int& inconsistency_count, Duration start_sleep, int id)
        : prev{prev}, inconsistency_count{inconsistency_count}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = ceq::rt::Now() + 10h;
      ceq::rt::SleepFor(start_sleep);
      while (ceq::rt::Now() < end_time) {
        ceq::rt::SleepFor(1s);
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

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host1);
  ceq::rt::AddHost("addr2", &host2, ceq::rt::HostOptions{.drift_interval = {0.001, 0.002}});
  ceq::rt::RunSimulation();

  EXPECT_GE(inconsistency_count, 5);
}

TEST(Clock, SleepLag) {
  struct Host final : public ceq::rt::IHostRunnable {
    explicit Host(int& prev, int& inconsistency_count, Duration start_sleep, int id)
        : prev{prev}, inconsistency_count{inconsistency_count}, start_sleep{start_sleep}, id{id} {
    }

    void Main() noexcept override {
      auto end_time = ceq::rt::Now() + 10h;
      ceq::rt::SleepFor(start_sleep);
      while (ceq::rt::Now() < end_time) {
        ceq::rt::SleepFor(1s);
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

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host1);
  ceq::rt::AddHost("addr2", &host2, ceq::rt::HostOptions{.max_sleep_lag = 1ms});
  ceq::rt::RunSimulation();

  EXPECT_GE(inconsistency_count, 5);
}

TEST(Clock, AwaitFiberInHost) {
  struct Host final : public ceq::rt::IHostRunnable {
    Host(Duration first_dur, Duration second_dur, Duration& last)
        : first_dur{first_dur}, second_dur{second_dur}, last{last} {
    }

    void Main() noexcept override {
      {
        boost::fibers::fiber handle(boost::fibers::launch::post, [&]() {
          ceq::rt::SleepFor(first_dur);
        });

        handle.join();
      }

      EXPECT_EQ(ceq::rt::Now().time_since_epoch(), first_dur);

      {
        boost::fibers::fiber handle(boost::fibers::launch::dispatch, [&]() {
          ceq::rt::SleepFor(second_dur);
        });

        handle.join();
      }

      auto current = ceq::rt::Now().time_since_epoch();
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

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host1);
  ceq::rt::AddHost("addr2", &host2);
  ceq::rt::AddHost("addr3", &host3);
  ceq::rt::AddHost("addr4", &host4);
  ceq::rt::AddHost("addr5", &host5);
  ceq::rt::RunSimulation();
}

TEST(Clock, Dispatch) {
  struct Host final : public ceq::rt::IHostRunnable {
    explicit Host(std::unordered_set<uint64_t>& hosts) : hosts{hosts} {
    }

    void Main() noexcept override {
      auto start_time = ceq::rt::Now();

      const auto host_id = ceq::rt::GetHostUniqueId();
      EXPECT_FALSE(hosts.contains(host_id));
      hosts.insert(host_id);

      auto h1 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        ceq::rt::SleepFor(1h);
        EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
        auto h1 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
          EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
          ceq::rt::SleepFor(1h);
          EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            ceq::rt::SleepFor(1h);
          }).wait();
          ceq::rt::SleepFor(1h);
          EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
        });

        auto h2 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
          ceq::rt::SleepFor(1h);
          EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            ceq::rt::SleepFor(1h);
          }).wait();
          ceq::rt::SleepFor(1h);
        });
        EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());

        ceq::rt::SleepFor(1h);
        h1.wait();
        ceq::rt::SleepFor(1h);
        h2.wait();
        ceq::rt::SleepFor(1h);
        EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
      });

      auto h2 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
        ceq::rt::SleepFor(1h);
        EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
        auto h1 = boost::fibers::async(boost::fibers::launch::post, [&]() {
          ceq::rt::SleepFor(1h);
          boost::fibers::async(boost::fibers::launch::dispatch, []() {
            ceq::rt::SleepFor(1h);
          }).wait();
          EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
          ceq::rt::SleepFor(1h);
        });

        auto h2 = boost::fibers::async(boost::fibers::launch::post, []() {
          ceq::rt::SleepFor(1h);
          boost::fibers::async(boost::fibers::launch::post, []() {
            ceq::rt::SleepFor(1h);
          }).wait();
          ceq::rt::SleepFor(1h);
        });

        ceq::rt::SleepFor(1h);
        h1.wait();
        ceq::rt::SleepFor(1h);
        h2.wait();
        ceq::rt::SleepFor(1h);
      });

      boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        ceq::rt::SleepFor(1h);
        EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
        h1.wait();
        ceq::rt::SleepFor(1h);
        EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
        h2.wait();
        ceq::rt::SleepFor(1h);
        EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
      }).wait();

      auto duration = ceq::rt::Now() - start_time;
      EXPECT_EQ(duration, 8h);
      EXPECT_EQ(host_id, ceq::rt::GetHostUniqueId());
    }

    std::unordered_set<uint64_t>& hosts;
  };

  for (uint64_t seed = 0; seed < 100; ++seed) {
    std::unordered_set<uint64_t> hosts;
    Host host(hosts);

    ceq::rt::HostOptions options{
        .start_time_interval = {1h, 2h},
        .drift_interval = {0.001, 0.002},
    };

    ceq::rt::InitWorld(42);
    for (size_t i = 0; i < 10; ++i) {
      ceq::rt::AddHost("addr" + std::to_string(i), &host, options);
    }
    ceq::rt::RunSimulation();
  }
}
