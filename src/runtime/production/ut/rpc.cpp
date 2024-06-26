#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>

#include <runtime/production/ut/test_service.client.h>
#include <runtime/production/ut/test_service.pb.h>
#include <runtime/production/ut/test_service.service.h>

using namespace std::chrono_literals;

using namespace mtf::rt;  // NOLINT

TEST(ProductionRpc, SimplyWorks) {
  struct EchoService final : public rpc::EchoServiceStub {
    mtf::Result<EchoReply, rpc::RpcError> Echo(const EchoRequest& request) noexcept override {
      EchoReply reply;
      reply.set_msg("Hello, " + request.msg());
      return mtf::Ok(std::move(reply));
    }
  };

  rpc::Server server;
  EchoService service;
  server.Register(&service);

  server.Start(10050);
  std::thread worker([&]() {
    server.Run();
  });

  SleepFor(500ms);

  rpc::EchoServiceClient client(Endpoint{"127.0.0.1", 10050});
  EchoRequest request;
  request.set_msg("Client");
  auto result = client.Echo(request);
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(result.GetValue().msg(), "Hello, Client");

  server.ShutDown();
  worker.join();
}

TEST(ProductionRpc, CancelSimplyWorks) {
  struct EchoService final : public rpc::EchoServiceStub {
    mtf::Result<EchoReply, rpc::RpcError> Echo(const EchoRequest& request) noexcept override {
      EchoReply reply;
      SleepFor(2s);
      reply.set_msg("Hello, " + request.msg());
      return mtf::Ok(std::move(reply));
    }
  };

  EchoService service;

  rpc::Server server;
  server.Register(&service);

  server.Start(10050);
  std::thread worker([&]() {
    server.Run();
  });

  SleepFor(500ms);

  StopSource stop;

  auto start = Now();

  boost::fibers::fiber stop_task([&]() {
    SleepFor(500ms);
    stop.Stop();
  });

  rpc::EchoServiceClient client(Endpoint{"127.0.0.1", 10050});
  EchoRequest request;
  request.set_msg("Client");
  auto result = client.Echo(request, stop.GetToken());

  EXPECT_TRUE(result.HasError());
  EXPECT_EQ(result.GetError().error_type, rpc::RpcErrorType::Cancelled);

  EXPECT_LT(Now() - start, 1s);
  stop_task.join();

  server.ShutDown();
  worker.join();
}

TEST(ProductionRpc, HandlerNotFound) {
  rpc::Server server;

  server.Start(10050);
  std::thread worker([&]() {
    server.Run();
  });

  SleepFor(500ms);

  rpc::EchoServiceClient client(Endpoint{"127.0.0.1", 10050});
  EchoRequest request;
  request.set_msg("Client");
  auto result = client.Echo(request);

  EXPECT_TRUE(result.HasError());
  EXPECT_EQ(result.GetError().error_type, rpc::RpcErrorType::HandlerNotFound);

  server.ShutDown();
  worker.join();
}

TEST(ProductionRpc, ConnectionRefused) {
  rpc::EchoServiceClient client(Endpoint{"127.0.0.1", 10050});
  EchoRequest request;
  request.set_msg("Client");
  auto result = client.Echo(request);

  EXPECT_TRUE(result.HasError());
  EXPECT_EQ(result.GetError().error_type, rpc::RpcErrorType::ConnectionRefused);
}
