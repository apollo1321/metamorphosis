#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <runtime/simulator/ut/test_service.client.h>
#include <runtime/simulator/ut/test_service.pb.h>
#include <runtime/simulator/ut/test_service.service.h>

using namespace std::chrono_literals;

using namespace ceq::rt;  // NOLINT

TEST(RuntimeSimulatorHostConnections, SimplyWorks) {
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

  struct Client final : public IHostRunnable {
    void Main() noexcept override {
      auto make_request = [](Address addr) {
        EchoServiceClient client({addr, 42});
        EchoRequest request;
        request.set_msg(addr);
        return client.Echo(request);
      };

      EXPECT_EQ(make_request("addr1").GetValue().msg(), "addr1");
      EXPECT_EQ(make_request("addr2").GetValue().msg(), "addr2");

      SleepFor(5s);

      EXPECT_EQ(make_request("addr1").GetError().error_type, RpcError::ErrorType::NetworkError);
      EXPECT_EQ(make_request("addr2").GetValue().msg(), "addr2");

      SleepFor(5s);

      EXPECT_EQ(make_request("addr1").GetValue().msg(), "addr1");
      EXPECT_EQ(make_request("addr2").GetValue().msg(), "addr2");

      SleepFor(5s);

      EXPECT_EQ(make_request("addr1").GetError().error_type, RpcError::ErrorType::NetworkError);
      EXPECT_EQ(make_request("addr2").GetError().error_type, RpcError::ErrorType::NetworkError);

      SleepFor(5s);

      EXPECT_EQ(make_request("addr1").GetValue().msg(), "addr1");
      EXPECT_EQ(make_request("addr2").GetValue().msg(), "addr2");
    }
  };

  struct Supervisor final : public IHostRunnable {
    void Main() noexcept override {
      SleepFor(2s);
      CloseLink("cli", "addr1");
      SleepFor(5s);
      RestoreLink("cli", "addr1");
      SleepFor(5s);
      CloseLink("cli", "addr1");
      CloseLink("cli", "addr2");
      SleepFor(5s);
      RestoreLink("cli", "addr1");
      RestoreLink("cli", "addr2");
    }
  };

  Host host;
  Client client;
  Supervisor supervisor;

  InitWorld(42);
  AddHost("addr1", &host);
  AddHost("addr2", &host);
  AddHost("cli", &client);
  AddHost("Supervisor", &supervisor);

  RunSimulation();
}
