#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime_simulator/api.h>
#include <runtime_simulator/rpc_server.h>

#include <runtime_simulator/ut/test_service.pb.h>
#include <runtime_simulator/ut/test_service.sim.client.h>
#include <runtime_simulator/ut/test_service.sim.service.h>

using namespace std::chrono_literals;

using runtime_simulation::Duration;
using runtime_simulation::RpcError;
using runtime_simulation::RpcServer;
using runtime_simulation::Timestamp;

TEST(Rpc, SimplyWorks) {
  struct EchoService final : public runtime_simulation::EchoServiceStub {
    Result<EchoReply, RpcError> Echo(const EchoRequest& request) noexcept override {
      using Result = Result<EchoReply, RpcError>;

      EchoReply reply;
      reply.set_msg("Hello, " + request.msg());

      return Result::Ok(std::move(reply));
    }
  };

  struct Host final : public runtime_simulation::IHostRunnable {
    void operator()() override {
      RpcServer server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      runtime_simulation::sleep_for(1h);
    }
  };

  struct Client final : public runtime_simulation::IHostRunnable {
    void operator()() override {
      runtime_simulation::sleep_for(1s);  // Wait for server to start up
      runtime_simulation::EchoServiceClient client("addr1", 42);

      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      std::cerr << result.ExpectValue().msg() << std::endl;
      EXPECT_EQ(result.ExpectValue().msg(), "Hello, Client");
    }
  };

  Host host;
  Client client;

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost("addr1", &host);
  runtime_simulation::AddHost("addr2", &client);
  runtime_simulation::RunSimulation();
}
