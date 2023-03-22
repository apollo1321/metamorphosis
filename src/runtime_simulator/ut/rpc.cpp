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
using runtime_simulation::WorldOptions;

struct EchoService final : public runtime_simulation::EchoServiceStub {
  Result<EchoReply, RpcError> Echo(const EchoRequest& request) noexcept override {
    using Result = Result<EchoReply, RpcError>;

    EchoReply reply;
    reply.set_msg("Hello, " + request.msg());

    return Result::Ok(std::move(reply));
  }
};

TEST(Rpc, SimplyWorks) {
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
      EXPECT_EQ(result.ExpectValue().msg(), "Hello, Client");

      request.set_msg("Again Client");
      result = client.Echo(request);
      EXPECT_EQ(result.ExpectValue().msg(), "Hello, Again Client");
    }
  };

  Host host;
  Client client;

  runtime_simulation::InitWorld(42);
  runtime_simulation::AddHost("addr1", &host);
  runtime_simulation::AddHost("addr2", &client);
  runtime_simulation::RunSimulation();
}

TEST(Rpc, DeliveryTimeOverhead) {
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

      auto start = runtime_simulation::now();

      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      EXPECT_EQ(result.ExpectValue().msg(), "Hello, Client");

      auto duration1 = runtime_simulation::now() - start;
      EXPECT_LE(duration1, 20ms);
      EXPECT_GE(duration1, 10ms);

      start = runtime_simulation::now();

      request.set_msg("Again Client");
      result = client.Echo(request);
      EXPECT_EQ(result.ExpectValue().msg(), "Hello, Again Client");

      auto duration2 = runtime_simulation::now() - start;
      EXPECT_LE(duration2, 20ms);
      EXPECT_GE(duration2, 10ms);
      EXPECT_NE(duration2, duration1);
    }
  };

  Host host;
  Client client;

  runtime_simulation::InitWorld(42,
                                WorldOptions{.min_delivery_time = 5ms, .max_delivery_time = 10ms});
  runtime_simulation::AddHost("addr1", &host);
  runtime_simulation::AddHost("addr2", &client);
  runtime_simulation::RunSimulation();
}

TEST(Rpc, NetworkErrorProba) {
  struct Host final : public runtime_simulation::IHostRunnable {
    void operator()() override {
      RpcServer server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      runtime_simulation::sleep_for(10h);
    }
  };

  struct Client final : public runtime_simulation::IHostRunnable {
    void operator()() override {
      runtime_simulation::sleep_for(1s);  // Wait for server to start up
      runtime_simulation::EchoServiceClient client("addr1", 42);

      size_t error_count = 0;
      for (size_t i = 0; i < 10000; ++i) {
        EchoRequest request;
        request.set_msg("Client");
        auto result = client.Echo(request);
        if (result.HasError()) {
          EXPECT_EQ(result.ExpectError().error_type,
                    runtime_simulation::RpcError::ErrorType::NetworkError);
          ++error_count;
        } else {
          EXPECT_EQ(result.ExpectValue().msg(), "Hello, Client");
        }
      }

      EXPECT_GE(error_count, 2500);
      EXPECT_LE(error_count, 3500);
    }
  };

  Host host;
  Client client;

  runtime_simulation::InitWorld(
      3, WorldOptions{
             .min_delivery_time = 5ms, .max_delivery_time = 10ms, .network_error_proba = 0.3});
  runtime_simulation::AddHost("addr1", &host);
  runtime_simulation::AddHost("addr2", &client);
  runtime_simulation::RunSimulation();
}
