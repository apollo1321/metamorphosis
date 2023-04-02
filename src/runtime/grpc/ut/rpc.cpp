#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/rpc_server.h>

#include <runtime/cancellation/stop_source.h>

#include <runtime/grpc/ut/test_service.client.h>
#include <runtime/grpc/ut/test_service.pb.h>
#include <runtime/grpc/ut/test_service.service.h>

using namespace std::chrono_literals;

using namespace ceq::rt;  // NOLINT

TEST(Rpc, SimplyWorks) {
  struct EchoService final : public ceq::rt::EchoServiceStub {
    ceq::Result<EchoReply, RpcError> Echo(const EchoRequest& request) noexcept override {
      EchoReply reply;
      reply.set_msg("Hello, " + request.msg());
      return ceq::Ok(std::move(reply));
    }
  };

  RpcServer server;
  EchoService service;
  server.Register(&service);

  server.Run(10050);

  SleepFor(500ms);

  EchoServiceClient client("127.0.0.1:10050");
  EchoRequest request;
  request.set_msg("Client");
  auto result = client.Echo(request);
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(result.GetValue().msg(), "Hello, Client");

  server.ShutDown();
}

TEST(Rpc, CancelSimplyWorks) {
  struct EchoService final : public ceq::rt::EchoServiceStub {
    ceq::Result<EchoReply, RpcError> Echo(const EchoRequest& request) noexcept override {
      EchoReply reply;
      SleepFor(2s);
      reply.set_msg("Hello, " + request.msg());
      return ceq::Ok(std::move(reply));
    }
  };

  EchoService service;

  RpcServer server;
  server.Register(&service);

  server.Run(10050);

  SleepFor(500ms);

  StopSource stop;

  auto start = Now();

  boost::fibers::fiber stop_task([&]() {
    SleepFor(500ms);
    stop.Stop();
  });

  EchoServiceClient client("127.0.0.1:10050");
  EchoRequest request;
  request.set_msg("Client");
  auto result = client.Echo(request, stop.GetToken());

  EXPECT_TRUE(result.HasError());
  EXPECT_EQ(result.GetError().error_type, RpcError::ErrorType::Cancelled);

  EXPECT_LT(Now() - start, 1s);
  stop_task.join();

  server.ShutDown();
}

TEST(Rpc, HandlerNotFound) {
  RpcServer server;

  server.Run(10050);

  SleepFor(500ms);

  EchoServiceClient client("127.0.0.1:10050");
  EchoRequest request;
  request.set_msg("Client");
  auto result = client.Echo(request);

  EXPECT_TRUE(result.HasError());
  EXPECT_EQ(result.GetError().error_type, RpcError::ErrorType::HandlerNotFound);

  server.ShutDown();
}

TEST(Rpc, ConnectionRefused) {
  EchoServiceClient client("127.0.0.1:10051");
  EchoRequest request;
  request.set_msg("Client");
  auto result = client.Echo(request);

  EXPECT_TRUE(result.HasError());
  EXPECT_EQ(result.GetError().error_type, RpcError::ErrorType::ConnectionRefused);
}
