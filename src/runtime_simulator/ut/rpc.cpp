#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime_simulator/api.h>
#include <runtime_simulator/rpc_server.h>

#include <runtime_simulator/ut/test_service.pb.h>
#include <runtime_simulator/ut/test_service.sim.client.h>
#include <runtime_simulator/ut/test_service.sim.service.h>

using namespace std::chrono_literals;

using runtime_simulation::Address;
using runtime_simulation::Duration;
using runtime_simulation::EchoServiceClient;
using runtime_simulation::Port;
using runtime_simulation::RpcError;
using runtime_simulation::RpcServer;
using runtime_simulation::Timestamp;
using runtime_simulation::WorldOptions;

struct EchoService final : public runtime_simulation::EchoServiceStub {
  explicit EchoService(std::string msg = "") : msg{msg} {
  }

  Result<EchoReply, RpcError> Echo(const EchoRequest& request) noexcept override {
    using Result = Result<EchoReply, RpcError>;

    EchoReply reply;
    reply.set_msg("Hello, " + request.msg());
    if (!msg.empty()) {
      *reply.mutable_msg() += msg;
    }

    return Result::Ok(std::move(reply));
  }

  std::string msg;
};

TEST(Rpc, SimplyWorks) {
  struct Host final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      runtime_simulation::sleep_for(1h);
    }
  };

  struct Client final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
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

TEST(Rpc, DeliveryTime) {
  struct Host final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      runtime_simulation::sleep_for(1h);
    }
  };

  struct Client final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
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
    void Main() noexcept override {
      RpcServer server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      runtime_simulation::sleep_for(10h);
    }
  };

  struct Client final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
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

TEST(Rpc, ManyClientsManyServers) {
  struct Host final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      RpcServer server1;
      RpcServer server2;

      auto handle = boost::fibers::async([]() {
        RpcServer server3;
        EchoService service3;

        server3.Register(&service3);
        server3.Run(3);

        runtime_simulation::sleep_for(10h);
      });

      EchoService service1;
      EchoService service2;

      server1.Register(&service1);
      server1.Run(1);

      server2.Register(&service2);
      server2.Run(2);

      runtime_simulation::sleep_for(10h);

      handle.wait();
    }
  };

  struct Client final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      runtime_simulation::sleep_for(1s);

      EchoServiceClient host1client1("addr1", 1);
      EchoServiceClient host1client2("addr1", 2);
      EchoServiceClient host1client3("addr1", 3);

      EchoServiceClient host2client1("addr2", 1);
      EchoServiceClient host2client2("addr2", 2);
      EchoServiceClient host2client3("addr2", 3);

      auto start_time = runtime_simulation::now();

      constexpr size_t kIterCount = 100;

      for (size_t i = 0; i < kIterCount; ++i) {
        std::vector<boost::fibers::future<void>> requests;
        auto start_request = [&](auto& client, std::string msg) {
          requests.emplace_back(boost::fibers::async([&client, msg]() {
            EchoRequest request;
            request.set_msg(msg);
            auto result = client.Echo(request);
            EXPECT_FALSE(result.HasError()) << result.ExpectError().Message();
            EXPECT_EQ(result.ExpectValue().msg(), "Hello, " + msg);
          }));
        };

        start_request(host1client1, "host1port1");
        start_request(host1client2, "host1port2");
        start_request(host1client3, "host1port3");

        start_request(host2client1, "host2port1");
        start_request(host2client2, "host2port2");
        start_request(host2client3, "host2port3");

        for (auto& request : requests) {
          request.wait();
        }
      }

      auto duration = runtime_simulation::now() - start_time;

      EXPECT_LT(duration, 20ms * kIterCount);
      EXPECT_GT(duration, 10ms * kIterCount);
    }
  };

  Host host;
  Client client;

  runtime_simulation::InitWorld(42,
                                WorldOptions{.min_delivery_time = 5ms, .max_delivery_time = 10ms});
  runtime_simulation::AddHost("addr1", &host);
  runtime_simulation::AddHost("addr2", &host);

  for (size_t i = 0; i < 50; ++i) {
    runtime_simulation::AddHost("client" + std::to_string(i), &client);
  }

  runtime_simulation::RunSimulation();
}

TEST(Rpc, EchoProxy) {
  struct ProxyService final : public runtime_simulation::EchoProxyStub {
    Result<EchoReply, RpcError> Forward1(const EchoRequest& request) noexcept override {
      runtime_simulation::EchoServiceClient client("addr1", 1);
      auto reply = client.Echo(request);
      *reply.ExpectValue().mutable_msg() += " Forward1";
      return reply;
    }

    Result<EchoReply, RpcError> Forward2(const EchoRequest& request) noexcept override {
      runtime_simulation::EchoServiceClient client("addr2", 2);
      auto reply = client.Echo(request);
      *reply.ExpectValue().mutable_msg() += " Forward2";
      return reply;
    }
  };

  struct ProxyHost final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;
      ProxyService service;
      server.Register(&service);
      server.Run(42);
      runtime_simulation::sleep_for(10h);
    }
  };

  struct EchoHost1 final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;
      EchoService service("host1");
      server.Register(&service);
      server.Run(1);
      runtime_simulation::sleep_for(10h);
    }
  };

  struct EchoHost2 final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;
      EchoService service("host2");
      server.Register(&service);
      server.Run(2);
      runtime_simulation::sleep_for(10h);
    }
  };

  struct Client final : public runtime_simulation::IHostRunnable {
    void Main() noexcept override {
      runtime_simulation::sleep_for(1s);

      runtime_simulation::EchoProxyClient client("proxy_addr", 42);

      auto start_time = runtime_simulation::now();

      constexpr size_t kIterCount = 100;

      boost::fibers::fiber f;
      auto handle1 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        for (size_t i = 0; i < kIterCount; ++i) {
          runtime_simulation::sleep_for(10ms);
          auto h = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
            runtime_simulation::sleep_for(1ms);
            EchoRequest request;
            request.set_msg("m=1;client=" + std::to_string(client_ind) +
                            ";ind=" + std::to_string(i));
            auto res = client.Forward1(request);
            EXPECT_FALSE(res.HasError()) << res.ExpectError().Message();
            EXPECT_EQ(res.ExpectValue().msg(), "Hello, m=1;client=" + std::to_string(client_ind) +
                                                   ";ind=" + std::to_string(i) + "host1 Forward1");
            runtime_simulation::sleep_for(1ms);
          });
          runtime_simulation::sleep_for(10ms);
          h.wait();
        }
      });


      auto handle2 = boost::fibers::async([&]() {
        for (size_t i = 0; i < kIterCount; ++i) {
          runtime_simulation::sleep_for(10ms);
          EchoRequest request;
          request.set_msg("m=2;client=" + std::to_string(client_ind) + ";ind=" + std::to_string(i));
          auto res = client.Forward2(request);
          EXPECT_FALSE(res.HasError()) << res.ExpectError().Message();
          EXPECT_EQ(res.ExpectValue().msg(), "Hello, m=2;client=" + std::to_string(client_ind) +
                                                 ";ind=" + std::to_string(i) + "host2 Forward2");
          runtime_simulation::sleep_for(10ms);
        }
      });

      handle1.wait();
      handle2.wait();

      auto duration = runtime_simulation::now() - start_time;

      EXPECT_LT(duration, (40ms + 22ms) * kIterCount);
      EXPECT_GT(duration, (20ms + 22ms) * kIterCount);
    }

    size_t client_ind = 0;
  };

  EchoHost1 host1;
  EchoHost2 host2;
  ProxyHost proxy_host;

  std::vector<std::unique_ptr<Client>> clients;

  runtime_simulation::InitWorld(42,
                                WorldOptions{.min_delivery_time = 5ms, .max_delivery_time = 10ms});
  runtime_simulation::AddHost("addr1", &host1);
  runtime_simulation::AddHost("addr2", &host2);
  runtime_simulation::AddHost("proxy_addr", &proxy_host);

  for (size_t i = 0; i < 50; ++i) {
    clients.emplace_back(std::make_unique<Client>());
    clients.back()->client_ind = i;
    runtime_simulation::AddHost("client" + std::to_string(i), clients.back().get());
  }

  runtime_simulation::RunSimulation();
}
