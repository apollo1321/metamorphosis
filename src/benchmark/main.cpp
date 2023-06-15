#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <benchmark/echo_service.client.h>
#include <benchmark/echo_service.service.h>

#include <runtime/api.h>

#include <CLI/CLI.hpp>

using namespace std::chrono_literals;

using ceq::Result;
using namespace ceq::rt;  // NOLINT

class EchoService final : public rpc::EchoServiceStub {
  Result<EchoReply, rpc::RpcError> SayHello(const EchoRequest& request) noexcept override {
    EchoReply reply;
    reply.set_message(request.name());
    return ceq::Ok(reply);
  }
};

struct ClientConfig {
  size_t fiber_count;
  size_t thread_count;
};

void BenchEchoService(ClientConfig client_config, uint16_t port) {
  std::cout << "=================================\n";
  std::cout << "Clients config: \n";
  std::cout << "\tfiber_count = " << client_config.fiber_count << '\n';
  std::cout << "\tthread_count = " << client_config.thread_count << '\n';

  EchoService service;
  rpc::Server server;
  server.Register(&service);

  server.Start(port);
  boost::fibers::fiber worker([&]() {
    server.Run();
  });

  std::atomic<bool> running{true};
  std::atomic<size_t> count{};

  rpc::EchoServiceClient client(Endpoint{"[::]", port});

  std::vector<std::thread> threads;
  boost::fibers::barrier barrier(client_config.thread_count + 1);
  for (size_t thread_id = 0; thread_id < client_config.thread_count; ++thread_id) {
    threads.emplace_back([&, thread_id]() {
      std::vector<boost::fibers::fiber> fibers;

      for (size_t fiber_id = 0; fiber_id < client_config.fiber_count; ++fiber_id) {
        fibers.emplace_back(boost::fibers::launch::post, [&, fiber_id, thread_id]() {
          while (running) {
            EchoRequest request;
            request.set_name(std::to_string(thread_id) + ":" + std::to_string(fiber_id));
            VERIFY(client.SayHello(request).GetValue().message() == request.name(),
                   "invalid response");
            count.fetch_add(1);
          }
        });
      }

      for (auto& fiber : fibers) {
        fiber.join();
      }

      barrier.wait();
    });
  }

  std::cout << "Warmup (5s)\n";
  SleepFor(5s);

  std::cout << "Benchmarking (5s)\n";
  count = 0;
  auto start_time = std::chrono::steady_clock::now();
  SleepFor(5s);
  auto end_time = std::chrono::steady_clock::now();
  size_t count_result = count;

  std::chrono::duration<double> duration = end_time - start_time;
  std::cout << "RPS: " << count_result / duration.count() << "\n\n";

  std::cout << "Stoping clients\n";

  running = false;
  barrier.wait();

  for (auto& thread : threads) {
    thread.join();
  }

  std::cout << "Stoping server\n";
  server.ShutDown();
  worker.join();
}

int main(int argc, char** argv) {
  CLI::App app{"Echo service benchmark"};

  uint16_t port;
  app.add_option("-p,--port", port, "free port")->default_val(10050);

  ClientConfig client_config{};
  auto client = app.add_subcommand("client", "client config");

  client->add_option("-t,--threads", client_config.thread_count, "threads count")->default_val(1);
  client->add_option("-f,--fibers", client_config.fiber_count, "fibers per thread")
      ->default_val(500);

  CLI11_PARSE(app, argc, argv);

  BenchEchoService(client_config, port);
}
