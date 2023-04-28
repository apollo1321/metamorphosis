#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <runtime/simulator/ut/test_service.client.h>
#include <runtime/simulator/ut/test_service.pb.h>
#include <runtime/simulator/ut/test_service.service.h>

using namespace std::chrono_literals;
using namespace ceq::rt;  // NOLINT

TEST(SimulatorHostConnections, SimplyWorks) {
  struct Host final : public sim::IHostRunnable, public rpc::EchoServiceStub {
    void Main() noexcept override {
      rpc::Server server;
      server.Register(this);
      server.Run(42);
      SleepFor(10h);
      server.ShutDown();
    }

    ceq::Result<EchoReply, rpc::RpcError> Echo(const EchoRequest& request) noexcept override {
      EchoReply reply;
      reply.set_msg(request.msg());
      return ceq::Ok(std::move(reply));
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      auto make_request = [](Address addr) {
        rpc::EchoServiceClient client({addr, 42});
        EchoRequest request;
        request.set_msg(addr);
        return client.Echo(request);
      };

      EXPECT_EQ(make_request("addr1").GetValue().msg(), "addr1");
      EXPECT_EQ(make_request("addr2").GetValue().msg(), "addr2");

      SleepFor(5s);

      EXPECT_EQ(make_request("addr1").GetError().error_type, rpc::RpcErrorType::NetworkError);
      EXPECT_EQ(make_request("addr2").GetValue().msg(), "addr2");

      SleepFor(5s);

      EXPECT_EQ(make_request("addr1").GetValue().msg(), "addr1");
      EXPECT_EQ(make_request("addr2").GetValue().msg(), "addr2");

      SleepFor(5s);

      EXPECT_EQ(make_request("addr1").GetError().error_type, rpc::RpcErrorType::NetworkError);
      EXPECT_EQ(make_request("addr2").GetError().error_type, rpc::RpcErrorType::NetworkError);

      SleepFor(5s);

      EXPECT_EQ(make_request("addr1").GetValue().msg(), "addr1");
      EXPECT_EQ(make_request("addr2").GetValue().msg(), "addr2");
    }
  };

  struct Supervisor final : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(2s);
      sim::CloseLink("cli", "addr1");
      SleepFor(5s);
      sim::RestoreLink("cli", "addr1");
      SleepFor(5s);
      sim::CloseLink("cli", "addr1");
      sim::CloseLink("cli", "addr2");
      SleepFor(5s);
      sim::RestoreLink("cli", "addr1");
      sim::RestoreLink("cli", "addr2");
    }
  };

  Host host;
  Client client;
  Supervisor supervisor;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &host);
  sim::AddHost("cli", &client);
  sim::AddHost("Supervisor", &supervisor);

  sim::RunSimulation();
}
