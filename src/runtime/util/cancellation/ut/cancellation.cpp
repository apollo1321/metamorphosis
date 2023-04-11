#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include <runtime/util/cancellation/stop_callback.h>
#include <runtime/util/cancellation/stop_source.h>
#include <runtime/util/cancellation/stop_token.h>

using namespace ceq::rt;  // NOLINT

TEST(Cancellation, SimplyWorks) {
  StopSource source;

  size_t run_cnt = 0;

  StopCallback cb(source.GetToken(), [&]() {
    ++run_cnt;
  });

  EXPECT_EQ(run_cnt, 0);

  source.Stop();
  source.Stop();

  EXPECT_EQ(run_cnt, 1);
}

TEST(Cancellation, CallbackAfterCancel) {
  StopSource source;

  source.Stop();

  size_t run_cnt = 0;

  StopCallback cb(source.GetToken(), [&]() {
    ++run_cnt;
  });

  EXPECT_EQ(run_cnt, 1);
}

TEST(Cancellation, MultipleCallbacks) {
  StopSource source;

  size_t run_cnt = 0;

  auto cb = [&]() {
    ++run_cnt;
  };

  StopCallback cb1(source.GetToken(), cb);
  StopCallback cb2(source.GetToken(), cb);
  StopCallback cb3(source.GetToken(), cb);
  StopCallback cb4(source.GetToken(), cb);
  StopCallback cb5(source.GetToken(), cb);
  StopCallback cb6(source.GetToken(), cb);
  StopCallback cb7(source.GetToken(), cb);

  EXPECT_EQ(run_cnt, 0);

  source.Stop();

  EXPECT_EQ(run_cnt, 7);
}

TEST(Cancellation, CallbackDeregisters) {
  StopSource source;

  size_t run_cnt = 0;

  auto cb = [&]() {
    ++run_cnt;
  };

  StopCallback cb1(source.GetToken(), cb);
  {
    StopCallback cb2(source.GetToken(), cb);
    StopCallback cb3(source.GetToken(), cb);
  }
  StopCallback cb4(source.GetToken(), cb);

  EXPECT_EQ(run_cnt, 0);

  source.Stop();

  EXPECT_EQ(run_cnt, 2);
}

TEST(Cancellation, CancelForward) {
  StopSource source1;
  StopSource source2;

  size_t run_cnt = 0;

  StopCallback cb1(source1.GetToken(), [&]() {
    source2.Stop();
  });

  StopCallback cb2(source2.GetToken(), [&]() {
    ++run_cnt;
  });

  EXPECT_EQ(run_cnt, 0);

  source1.Stop();

  EXPECT_EQ(run_cnt, 1);
}

TEST(Cancellation, ConcurrentCallbackRegistration) {
  for (size_t i = 0; i < 50; ++i) {
    StopSource source;

    auto registration_loop = [&]() {
      std::atomic_bool cancelled{false};

      auto cb = [&]() {
        cancelled = true;
      };

      StopCallback cb1(source.GetToken(), cb);

      while (!cancelled) {
        StopCallback cb2(source.GetToken(), cb);
        StopCallback cb3(source.GetToken(), cb);
        StopCallback cb4(source.GetToken(), cb);
        StopCallback cb5(source.GetToken(), cb);
        StopCallback cb6(source.GetToken(), cb);
      }
    };

    auto cancel_cb = [&]() {
      source.Stop();
    };

    std::vector<std::thread> threads;

    for (size_t ind = 0; ind < 5; ++ind) {
      threads.emplace_back(registration_loop);
    }

    for (size_t ind = 0; ind < 3; ++ind) {
      threads.emplace_back(cancel_cb);
    }

    for (auto& thread : threads) {
      thread.join();
    }
  }
}

TEST(Cancellation, SourceMove) {
  StopSource source;
  auto token1 = source.GetToken();
  auto token2 = source.GetToken();
  source = StopSource{};
}

TEST(Cancellation, TokenStopRequested) {
  StopSource source;
  auto token = source.GetToken();
  auto token1 = token;
  EXPECT_FALSE(token.StopRequested());
  EXPECT_FALSE(token1.StopRequested());
  source.Stop();
  EXPECT_TRUE(source.GetToken().StopRequested());
  EXPECT_TRUE(token.StopRequested());
  EXPECT_TRUE(token1.StopRequested());
}
