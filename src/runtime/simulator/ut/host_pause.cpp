#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <runtime/simulator/ut/test_service.client.h>
#include <runtime/simulator/ut/test_service.pb.h>
#include <runtime/simulator/ut/test_service.service.h>

using namespace std::chrono_literals;

using namespace ceq::rt;  // NOLINT

TEST(RuntimeSimulatorHostPause, SimplyWorks) {
  struct Supervisor : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);
      PauseHost("addr1");
      SleepFor(1h);
      ResumeHost("addr1");
    }
  };

  struct Host final : public IHostRunnable {
    void Main() noexcept override {
      auto start = ceq::rt::Now();
      ceq::rt::SleepFor(10s);
      auto end = ceq::rt::Now();
      EXPECT_EQ(end - start, 1h + 1s);
    }
  };

  Host host;
  Supervisor supervisor;

  InitWorld(42);
  AddHost("addr1", &host);
  AddHost("supervisor", &supervisor);
  RunSimulation();
}

TEST(RuntimeSimulatorHostPause, RpcCalls) {
  struct Supervisor : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1h);
      PauseHost("addr1");
      SleepFor(2h);
      ResumeHost("addr1");
    }
  };

  struct Client final : public IHostRunnable {
    void Main() noexcept override {
      EchoServiceClient client({"addr1", 42});

      {
        auto start = Now();
        EchoRequest request;
        request.set_msg("test");
        auto response = client.Echo(std::move(request));
        EXPECT_EQ(start, Now());
      }

      SleepFor(2h);

      {
        auto start = Now();
        EchoRequest request;
        request.set_msg("test1");
        auto response = client.Echo(std::move(request));
        EXPECT_EQ(start + 1h, Now());
      }

      {
        auto start = Now();
        EchoRequest request;
        request.set_msg("test2");
        auto response = client.Echo(std::move(request));
        EXPECT_EQ(start, Now());
      }
    }
  };

  struct Host final : public IHostRunnable, public EchoServiceStub {
    void Main() noexcept override {
      RpcServer server;
      server.Register(this);
      server.Run(42);
      SleepFor(10h);
      server.ShutDown();
    }

    ceq::Result<EchoReply, ceq::rt::RpcError> Echo(const EchoRequest& request) noexcept override {
      EchoReply reply;
      reply.set_msg(request.msg());
      return ceq::Ok(std::move(reply));
    }
  };

  Host host;
  Supervisor supervisor;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::AddHost("supervisor", &supervisor);
  ceq::rt::RunSimulation();
}
