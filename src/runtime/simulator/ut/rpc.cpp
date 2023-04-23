#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/api.h>
#include <runtime/simulator/api.h>

#include <runtime/simulator/ut/test_service.client.h>
#include <runtime/simulator/ut/test_service.pb.h>
#include <runtime/simulator/ut/test_service.service.h>

using namespace std::chrono_literals;
using namespace ceq::rt;  // NOLINT

struct EchoService final : public rpc::EchoServiceStub {
  explicit EchoService(std::string msg = "") : msg{msg} {
  }

  ceq::Result<EchoReply, rpc::Error> Echo(const EchoRequest& request) noexcept override {
    EchoReply reply;
    reply.set_msg("Hello, " + request.msg());
    if (!msg.empty()) {
      *reply.mutable_msg() += msg;
    }

    return ceq::Ok(std::move(reply));
  }

  std::string msg;
};

TEST(SimulatorRpc, SimplyWorks) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::Server server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      SleepFor(1h);
      server.ShutDown();
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);  // Wait for server to start up
      rpc::EchoServiceClient client(Endpoint{"addr1", 42});

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

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &client);
  sim::RunSimulation();
}

TEST(SimulatorRpc, DeliveryTime) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::Server server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      SleepFor(1h);

      server.ShutDown();
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);  // Wait for server to start up
      rpc::EchoServiceClient client(Endpoint{"addr1", 42});

      auto start = Now();

      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      EXPECT_EQ(result.GetValue().msg(), "Hello, Client");

      auto duration1 = Now() - start;
      EXPECT_LE(duration1, 20ms);
      EXPECT_GE(duration1, 10ms);

      start = Now();

      request.set_msg("Again Client");
      result = client.Echo(request);
      EXPECT_EQ(result.GetValue().msg(), "Hello, Again Client");

      auto duration2 = Now() - start;
      EXPECT_LE(duration2, 20ms);
      EXPECT_GE(duration2, 10ms);
      EXPECT_NE(duration2, duration1);
    }
  };

  Host host;
  Client client;

  sim::InitWorld(42, sim::WorldOptions{.delivery_time_interval = {5ms, 10ms}});
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &client);
  sim::RunSimulation();
}

TEST(SimulatorRpc, NetworkErrorProba) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::Server server;

      EchoService service;

      server.Register(&service);
      server.Run(42);

      SleepFor(10h);

      server.ShutDown();
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);  // Wait for server to start up
      rpc::EchoServiceClient client(Endpoint{"addr1", 42});

      size_t error_count = 0;
      for (size_t i = 0; i < 10000; ++i) {
        EchoRequest request;
        request.set_msg("Client");
        auto result = client.Echo(request);
        if (result.HasError()) {
          EXPECT_EQ(result.GetError().error_type, rpc::Error::ErrorType::NetworkError);
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

  sim::InitWorld(
      3, sim::WorldOptions{.delivery_time_interval = {5ms, 10ms}, .network_error_proba = 0.3});
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &client);
  sim::RunSimulation();
}

TEST(SimulatorRpc, ManyClientsManyServers) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      EchoService service1;
      EchoService service2;

      rpc::Server server1;
      rpc::Server server2;

      auto handle = boost::fibers::async([]() {
        EchoService service3;
        rpc::Server server3;

        server3.Register(&service3);
        server3.Run(3);

        SleepFor(10h);

        server3.ShutDown();
      });

      server1.Register(&service1);
      server1.Run(1);

      server2.Register(&service2);
      server2.Run(2);

      SleepFor(10h);

      handle.wait();

      server1.ShutDown();
      server2.ShutDown();
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);

      rpc::EchoServiceClient host1client1(Endpoint{"addr1", 1});
      rpc::EchoServiceClient host1client2(Endpoint{"addr1", 2});
      rpc::EchoServiceClient host1client3(Endpoint{"addr1", 3});

      rpc::EchoServiceClient host2client1(Endpoint{"addr2", 1});
      rpc::EchoServiceClient host2client2(Endpoint{"addr2", 2});
      rpc::EchoServiceClient host2client3(Endpoint{"addr2", 3});

      auto start_time = Now();

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

      auto duration = Now() - start_time;

      EXPECT_LT(duration, 20ms * kIterCount);
      EXPECT_GT(duration, 10ms * kIterCount);
    }
  };

  Host host;
  Client client;

  sim::InitWorld(42, sim::WorldOptions{.delivery_time_interval = {5ms, 10ms}});
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &host);

  for (size_t i = 0; i < 50; ++i) {
    sim::AddHost("client" + std::to_string(i), &client);
  }

  sim::RunSimulation();
}

TEST(SimulatorRpc, EchoProxy) {
  struct ProxyHost final : public sim::IHostRunnable, public rpc::EchoProxyStub {
    void Main() noexcept override {
      host_id = sim::GetHostUniqueId();
      rpc::Server server;
      server.Register(this);
      server.Run(42);
      SleepFor(10h);
      server.ShutDown();
    }

    ceq::Result<EchoReply, rpc::Error> Forward1(const EchoRequest& request) noexcept override {
      EXPECT_EQ(host_id, sim::GetHostUniqueId());
      rpc::EchoServiceClient client(Endpoint{"addr1", 1});
      auto reply = client.Echo(request);
      EXPECT_EQ(host_id, sim::GetHostUniqueId());
      *reply.GetValue().mutable_msg() += " Forward1";
      return reply;
    }

    ceq::Result<EchoReply, rpc::Error> Forward2(const EchoRequest& request) noexcept override {
      EXPECT_EQ(host_id, sim::GetHostUniqueId());
      rpc::EchoServiceClient client(Endpoint{"addr2", 2});
      auto reply = client.Echo(request);
      EXPECT_EQ(host_id, sim::GetHostUniqueId());
      *reply.GetValue().mutable_msg() += " Forward2";
      return reply;
    }

    uint64_t host_id{};
  };

  struct EchoHost1 final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::Server server;
      EchoService service("host1");
      server.Register(&service);
      server.Run(1);
      SleepFor(10h);
      server.ShutDown();
    }
  };

  struct EchoHost2 final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::Server server;
      EchoService service("host2");
      server.Register(&service);
      server.Run(2);
      SleepFor(10h);
      server.ShutDown();
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);

      rpc::EchoProxyClient client(Endpoint{"proxy_addr", 42});

      auto start_time = Now();

      constexpr size_t kIterCount = 100;

      boost::fibers::fiber f;
      auto handle1 = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
        for (size_t i = 0; i < kIterCount; ++i) {
          SleepFor(10ms);
          auto h = boost::fibers::async(boost::fibers::launch::dispatch, [&]() {
            SleepFor(1ms);
            EchoRequest request;
            request.set_msg("m=1;client=" + std::to_string(client_ind) +
                            ";ind=" + std::to_string(i));
            auto res = client.Forward1(request);
            EXPECT_FALSE(res.HasError()) << res.GetError().Message();
            EXPECT_EQ(res.GetValue().msg(), "Hello, m=1;client=" + std::to_string(client_ind) +
                                                ";ind=" + std::to_string(i) + "host1 Forward1");
            SleepFor(1ms);
          });
          SleepFor(10ms);
          h.wait();
        }
      });

      auto handle2 = boost::fibers::async([&]() {
        for (size_t i = 0; i < kIterCount; ++i) {
          SleepFor(10ms);
          EchoRequest request;
          request.set_msg("m=2;client=" + std::to_string(client_ind) + ";ind=" + std::to_string(i));
          auto res = client.Forward2(request);
          EXPECT_FALSE(res.HasError()) << res.GetError().Message();
          EXPECT_EQ(res.GetValue().msg(), "Hello, m=2;client=" + std::to_string(client_ind) +
                                              ";ind=" + std::to_string(i) + "host2 Forward2");
          SleepFor(10ms);
        }
      });

      handle1.wait();
      handle2.wait();

      auto duration = Now() - start_time;

      EXPECT_LT(duration, (40ms + 22ms) * kIterCount);
      EXPECT_GT(duration, (20ms + 22ms) * kIterCount);
    }

    size_t client_ind = 0;
  };

  EchoHost1 host1;
  EchoHost2 host2;
  ProxyHost proxy_host;

  std::vector<std::unique_ptr<Client>> clients;

  sim::InitWorld(42, sim::WorldOptions{.delivery_time_interval = {5ms, 10ms}});
  sim::AddHost("addr1", &host1);
  sim::AddHost("addr2", &host2);
  sim::AddHost("proxy_addr", &proxy_host);

  for (size_t i = 0; i < 50; ++i) {
    clients.emplace_back(std::make_unique<Client>());
    clients.back()->client_ind = i;
    sim::AddHost("client" + std::to_string(i), clients.back().get());
  }

  sim::RunSimulation();
}

TEST(SimulatorRpc, CancelSimplyWorks) {
  struct Host final : public sim::IHostRunnable, public rpc::EchoServiceStub {
    void Main() noexcept override {
      rpc::Server server;

      server.Register(this);
      server.Run(42);

      SleepFor(1h);
      server.ShutDown();
    }

    ceq::Result<EchoReply, rpc::Error> Echo(const EchoRequest& request) noexcept override {
      SleepFor(5s);
      return ceq::Err(rpc::Error::ErrorType::Internal);
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      SleepFor(1s);  // Wait for server to start up
      rpc::EchoServiceClient client(Endpoint{"addr1", 42});

      StopSource source;

      auto cancel_task = boost::fibers::async([&]() {
        SleepFor(1s);
        source.Stop();
      });

      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request, source.GetToken());
      EXPECT_EQ(result.GetError().error_type, rpc::Error::ErrorType::Cancelled);

      cancel_task.wait();
    }
  };

  Host host;
  Client client;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &client);
  sim::RunSimulation();
}

TEST(SimulatorRpc, HandlerNotFound) {
  struct Host final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::Server server;
      server.Run(42);
      SleepFor(1h);
      server.ShutDown();
    }
  };

  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::EchoServiceClient client(Endpoint{"addr1", 42});
      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      EXPECT_TRUE(result.HasError());
      EXPECT_EQ(result.GetError().error_type, rpc::Error::ErrorType::HandlerNotFound);
    }
  };

  Host host;
  Client client;

  sim::InitWorld(42);
  sim::AddHost("addr1", &host);
  sim::AddHost("addr2", &client);
  sim::RunSimulation();
}

TEST(SimulatorRpc, ConnectionRefused) {
  struct Client final : public sim::IHostRunnable {
    void Main() noexcept override {
      rpc::EchoServiceClient client(Endpoint{"addr1", 43});
      EchoRequest request;
      request.set_msg("Client");
      auto result = client.Echo(request);
      EXPECT_TRUE(result.HasError());
      EXPECT_EQ(result.GetError().error_type, rpc::Error::ErrorType::ConnectionRefused);
    }
  };

  Client client;

  sim::InitWorld(42);
  sim::AddHost("addr2", &client);
  sim::RunSimulation();
}
