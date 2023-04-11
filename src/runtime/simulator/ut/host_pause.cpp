#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <runtime/simulator/ut/test_service.client.h>
#include <runtime/simulator/ut/test_service.pb.h>
#include <runtime/simulator/ut/test_service.service.h>

using namespace std::chrono_literals;
using namespace ceq::rt;  // NOLINT

TEST(SimulatorHostPause, SimplyWorks) {
  struct Supervisor : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);
      sim::PauseHost("addr1");
      SleepFor(1h);
      sim::ResumeHost("addr1");
    }
  };

  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      auto start = Now();
      SleepFor(10s);
      auto end = Now();
      EXPECT_EQ(end - start, 1h + 1s);
    }
  };

  Host host;
  Supervisor supervisor;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::AddHost("supervisor", &supervisor);
  sim::RunSimulation();
}

TEST(SimulatorHostPause, PauseServer) {
  struct Supervisor : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1h);
      sim::PauseHost("addr1");
      SleepFor(2h);
      sim::ResumeHost("addr1");
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::EchoServiceClient client({"addr1", 42});

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

  struct Host final : public sim::IHostRunnable, public rpc::EchoServiceStub {
    void Main() noexcept override {
      rpc::Server server;
      server.Register(this);
      server.Run(42);
      SleepFor(10h);
      server.ShutDown();
    }

    ceq::Result<EchoReply, rpc::Error> Echo(const EchoRequest& request) noexcept override {
      EchoReply reply;
      reply.set_msg(request.msg());
      return ceq::Ok(std::move(reply));
    }
  };

  Host host;
  Client client;
  Supervisor supervisor;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &client);
  sim::AddHost("supervisor", &supervisor);
  sim::RunSimulation();
}

TEST(SimulatorHostPause, PauseClient) {
  struct Supervisor : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(30min);
      sim::PauseHost("addr2");
      SleepFor(2h);
      sim::ResumeHost("addr2");
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::EchoServiceClient client({"addr1", 42});

      auto start = Now();
      EchoRequest request;
      request.set_msg("test");
      auto response = client.Echo(std::move(request));
      EXPECT_EQ(start + 2h + 30min, Now());
    }
  };

  struct Host final : public sim::IHostRunnable, public rpc::EchoServiceStub {
    void Main() noexcept override {
      rpc::Server server;
      server.Register(this);
      server.Run(42);
      SleepFor(10h);
      server.ShutDown();
    }

    ceq::Result<EchoReply, rpc::Error> Echo(const EchoRequest& request) noexcept override {
      EchoReply reply;
      SleepFor(1h);
      reply.set_msg(request.msg());
      return ceq::Ok(std::move(reply));
    }
  };

  Host host;
  Client client;
  Supervisor supervisor;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &client);
  sim::AddHost("supervisor", &supervisor);
  sim::RunSimulation();
}
