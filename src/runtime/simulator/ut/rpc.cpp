#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/rpc_server.h>
#include <runtime/simulator/api.h>

#include <runtime/simulator/ut/test_service.client.h>
#include <runtime/simulator/ut/test_service.pb.h>
#include <runtime/simulator/ut/test_service.service.h>

#include <runtime/cancellation/stop_source.h>

using namespace std::chrono_literals;

using namespace ceq::rt;  // NOLINT

struct EchoService final : public ceq::rt::EchoServiceStub {
  explicit EchoService(std::string msg = "") : msg{msg} {
  }

  ceq::Result<EchoReply, RpcError> Echo(const EchoRequest& request) noexcept override {
    EchoReply reply;
    reply.set_msg("Hello, " + request.msg());
    if (!msg.empty()) {
      *reply.mutable_msg() += msg;
    }

    return ceq::Ok(std::move(reply));
  }

  std::string msg;
};

TEST(Rpc, SimplyWorks) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      ceq::rt::SleepFor(1h);
      server.ShutDown();
    }
  };

  struct Client final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::SleepFor(1s);  // Wait for server to start up
      ceq::rt::EchoServiceClient client(Endpoint{"addr1", 42});

      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      EXPECT_EQ(result.GetValue().msg(), "Hello, Client");

      request.set_msg("Again Client");
      result = client.Echo(request);
      EXPECT_EQ(result.GetValue().msg(), "Hello, Again Client");
    }
  };

  Host host;
  Client client;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::AddHost("addr2", &client);
  ceq::rt::RunSimulation();
}

TEST(Rpc, DeliveryTime) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      ceq::rt::SleepFor(1h);

      server.ShutDown();
    }
  };

  struct Client final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::SleepFor(1s);  // Wait for server to start up
      ceq::rt::EchoServiceClient client(Endpoint{"addr1", 42});

      auto start = ceq::rt::Now();

      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      EXPECT_EQ(result.GetValue().msg(), "Hello, Client");

      auto duration1 = ceq::rt::Now() - start;
      EXPECT_LE(duration1, 20ms);
      EXPECT_GE(duration1, 10ms);

      start = ceq::rt::Now();

      request.set_msg("Again Client");
      result = client.Echo(request);
      EXPECT_EQ(result.GetValue().msg(), "Hello, Again Client");

      auto duration2 = ceq::rt::Now() - start;
      EXPECT_LE(duration2, 20ms);
      EXPECT_GE(duration2, 10ms);
      EXPECT_NE(duration2, duration1);
    }
  };

  Host host;
  Client client;

  ceq::rt::InitWorld(42, WorldOptions{.delivery_time_interval = {5ms, 10ms}});
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::AddHost("addr2", &client);
  ceq::rt::RunSimulation();
}

TEST(Rpc, NetworkErrorProba) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      ceq::rt::SleepFor(10h);

      server.ShutDown();
    }
  };

  struct Client final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::SleepFor(1s);  // Wait for server to start up
      ceq::rt::EchoServiceClient client(Endpoint{"addr1", 42});

      size_t error_count = 0;
      for (size_t i = 0; i < 10000; ++i) {
        EchoRequest request;
        request.set_msg("Client");
        auto result = client.Echo(request);
        if (result.HasError()) {
          EXPECT_EQ(result.GetError().error_type, ceq::rt::RpcError::ErrorType::NetworkError);
          ++error_count;
        } else {
          EXPECT_EQ(result.GetValue().msg(), "Hello, Client");
        }
      }

      EXPECT_GE(error_count, 2500);
      EXPECT_LE(error_count, 3500);
    }
  };

  Host host;
  Client client;

  ceq::rt::InitWorld(
      3, WorldOptions{.delivery_time_interval = {5ms, 10ms}, .network_error_proba = 0.3});
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::AddHost("addr2", &client);
  ceq::rt::RunSimulation();
}

TEST(Rpc, ManyClientsManyServers) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      EchoService service1;
      EchoService service2;

      RpcServer server1;
      RpcServer server2;

      auto handle = boost::fibers::async([]() {
        EchoService service3;
        RpcServer server3;

        server3.Register(&service3);
        server3.Run(3);

        ceq::rt::SleepFor(10h);

        server3.ShutDown();
      });

      server1.Register(&service1);
      server1.Run(1);

      server2.Register(&service2);
      server2.Run(2);

      ceq::rt::SleepFor(10h);

      handle.wait();

      server1.ShutDown();
      server2.ShutDown();
    }
  };

  struct Client final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::SleepFor(1s);

      EchoServiceClient host1client1(Endpoint{"addr1", 1});
      EchoServiceClient host1client2(Endpoint{"addr1", 2});
      EchoServiceClient host1client3(Endpoint{"addr1", 3});

      EchoServiceClient host2client1(Endpoint{"addr2", 1});
      EchoServiceClient host2client2(Endpoint{"addr2", 2});
      EchoServiceClient host2client3(Endpoint{"addr2", 3});

      auto start_time = ceq::rt::Now();

      constexpr size_t kIterCount = 100;

      for (size_t i = 0; i < kIterCount; ++i) {
        std::vector<boost::fibers::future<void>> requests;
        auto start_request = [&](auto& client, std::string msg) {
          requests.emplace_back(boost::fibers::async([&client, msg]() {
            EchoRequest request;
            request.set_msg(msg);
            auto result = client.Echo(request);
            EXPECT_FALSE(result.HasError()) << result.GetError().Message();
            EXPECT_EQ(result.GetValue().msg(), "Hello, " + msg);
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

      auto duration = ceq::rt::Now() - start_time;

      EXPECT_LT(duration, 20ms * kIterCount);
      EXPECT_GT(duration, 10ms * kIterCount);
    }
  };

  Host host;
  Client client;

  ceq::rt::InitWorld(42, WorldOptions{.delivery_time_interval = {5ms, 10ms}});
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::AddHost("addr2", &host);

  for (size_t i = 0; i < 50; ++i) {
    ceq::rt::AddHost("client" + std::to_string(i), &client);
  }

  ceq::rt::RunSimulation();
}

TEST(Rpc, EchoProxy) {
  struct ProxyHost final : public ceq::rt::IHostRunnable, public ceq::rt::EchoProxyStub {
    void Main() noexcept override {
      host_id = GetHostUniqueId();
      RpcServer server;
      server.Register(this);
      server.Run(42);
      ceq::rt::SleepFor(10h);
      server.ShutDown();
    }

    ceq::Result<EchoReply, RpcError> Forward1(const EchoRequest& request) noexcept override {
      EXPECT_EQ(host_id, GetHostUniqueId());
      ceq::rt::EchoServiceClient client(Endpoint{"addr1", 1});
      auto reply = client.Echo(request);
      EXPECT_EQ(host_id, GetHostUniqueId());
      *reply.GetValue().mutable_msg() += " Forward1";
      return reply;
    }

    ceq::Result<EchoReply, RpcError> Forward2(const EchoRequest& request) noexcept override {
      EXPECT_EQ(host_id, GetHostUniqueId());
      ceq::rt::EchoServiceClient client(Endpoint{"addr2", 2});
      auto reply = client.Echo(request);
      EXPECT_EQ(host_id, GetHostUniqueId());
      *reply.GetValue().mutable_msg() += " Forward2";
      return reply;
    }

    uint64_t host_id{};
  };

  struct EchoHost1 final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;
      EchoService service("host1");
      server.Register(&service);
      server.Run(1);
      ceq::rt::SleepFor(10h);
      server.ShutDown();
    }
  };

  struct EchoHost2 final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      RpcServer server;
      EchoService service("host2");
      server.Register(&service);
      server.Run(2);
      ceq::rt::SleepFor(10h);
      server.ShutDown();
    }
  };

  struct Client final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::SleepFor(1s);

      ceq::rt::EchoProxyClient client(Endpoint{"proxy_addr", 42});

      auto start_time = ceq::rt::Now();

      constexpr size_t kIterCount = 100;

      boost::fibers::fiber f;
      auto handle1 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        for (size_t i = 0; i < kIterCount; ++i) {
          ceq::rt::SleepFor(10ms);
          auto h = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
            ceq::rt::SleepFor(1ms);
            EchoRequest request;
            request.set_msg("m=1;client=" + std::to_string(client_ind) +
                            ";ind=" + std::to_string(i));
            auto res = client.Forward1(request);
            EXPECT_FALSE(res.HasError()) << res.GetError().Message();
            EXPECT_EQ(res.GetValue().msg(), "Hello, m=1;client=" + std::to_string(client_ind) +
                                                ";ind=" + std::to_string(i) + "host1 Forward1");
            ceq::rt::SleepFor(1ms);
          });
          ceq::rt::SleepFor(10ms);
          h.wait();
        }
      });

      auto handle2 = boost::fibers::async([&]() {
        for (size_t i = 0; i < kIterCount; ++i) {
          ceq::rt::SleepFor(10ms);
          EchoRequest request;
          request.set_msg("m=2;client=" + std::to_string(client_ind) + ";ind=" + std::to_string(i));
          auto res = client.Forward2(request);
          EXPECT_FALSE(res.HasError()) << res.GetError().Message();
          EXPECT_EQ(res.GetValue().msg(), "Hello, m=2;client=" + std::to_string(client_ind) +
                                              ";ind=" + std::to_string(i) + "host2 Forward2");
          ceq::rt::SleepFor(10ms);
        }
      });

      handle1.wait();
      handle2.wait();

      auto duration = ceq::rt::Now() - start_time;

      EXPECT_LT(duration, (40ms + 22ms) * kIterCount);
      EXPECT_GT(duration, (20ms + 22ms) * kIterCount);
    }

    size_t client_ind = 0;
  };

  EchoHost1 host1;
  EchoHost2 host2;
  ProxyHost proxy_host;

  std::vector<std::unique_ptr<Client>> clients;

  ceq::rt::InitWorld(42, WorldOptions{.delivery_time_interval = {5ms, 10ms}});
  ceq::rt::AddHost("addr1", &host1);
  ceq::rt::AddHost("addr2", &host2);
  ceq::rt::AddHost("proxy_addr", &proxy_host);

  for (size_t i = 0; i < 50; ++i) {
    clients.emplace_back(std::make_unique<Client>());
    clients.back()->client_ind = i;
    ceq::rt::AddHost("client" + std::to_string(i), clients.back().get());
  }

  ceq::rt::RunSimulation();
}

TEST(Rpc, CancelSimplyWorks) {
  struct Host final : public ceq::rt::IHostRunnable, public ceq::rt::EchoServiceStub {
    void Main() noexcept override {
      RpcServer server;

      server.Register(this);
      server.Run(42);

      ceq::rt::SleepFor(1h);
      server.ShutDown();
    }

    ceq::Result<EchoReply, RpcError> Echo(const EchoRequest& request) noexcept override {
      ceq::rt::SleepFor(5s);
      return ceq::Err(RpcError(RpcError::ErrorType::Internal));
    }
  };

  struct Client final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::SleepFor(1s);  // Wait for server to start up
      ceq::rt::EchoServiceClient client(Endpoint{"addr1", 42});

      StopSource source;

      auto cancel_task = boost::fibers::async([&]() {
        SleepFor(1s);
        source.Stop();
      });

      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request, source.GetToken());
      EXPECT_EQ(result.GetError().error_type, RpcError::ErrorType::Cancelled);

      cancel_task.wait();
    }
  };

  Host host;
  Client client;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::AddHost("addr2", &client);
  ceq::rt::RunSimulation();
}

TEST(Rpc, HandlerNotFound) {
  struct Host final : public IHostRunnable {
    void Main() noexcept override {
      RpcServer server;
      server.Run(42);
      ceq::rt::SleepFor(1h);
      server.ShutDown();
    }
  };

  struct Client final : public IHostRunnable {
    void Main() noexcept override {
      EchoServiceClient client(Endpoint{"addr1", 42});
      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      EXPECT_TRUE(result.HasError());
      EXPECT_EQ(result.GetError().error_type, RpcError::ErrorType::HandlerNotFound);
    }
  };

  Host host;
  Client client;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::AddHost("addr2", &client);
  ceq::rt::RunSimulation();
}

TEST(Rpc, ConnectionRefused) {
  struct Client final : public IHostRunnable {
    void Main() noexcept override {
      EchoServiceClient client(Endpoint{"addr1", 43});
      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      EXPECT_TRUE(result.HasError());
      EXPECT_EQ(result.GetError().error_type, RpcError::ErrorType::ConnectionRefused);
    }
  };

  Client client;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr2", &client);
  ceq::rt::RunSimulation();
}
