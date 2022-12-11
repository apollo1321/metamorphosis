#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <proto/echo_service.client.h>
#include <proto/echo_service.handler.h>

using namespace std::chrono_literals;

class EchoServiceHandlerImpl final : public EchoServiceHandler {
  EchoReply SayHello(const EchoRequest& request) override {
    EchoReply reply;
    reply.set_message("Hello from async server " + request.name());
    return reply;
  }
};

struct ClientConfig {
  size_t fiber_count;
  size_t thread_count;
};

void BenchEchoService(RpcHandlerBase::RunConfig server_config, ClientConfig client_config,
                      uint16_t port) {
  std::cout << "=================================\n";
  std::cout << "Server config: \n";
  std::cout << "\tqueue_count = " << server_config.queue_count << '\n';
  std::cout << "\tthreads_per_queue = " << server_config.threads_per_queue << '\n';
  std::cout << "\tworker_threads_count = " << server_config.worker_threads_count << '\n';
  std::cout << "Clients config: \n";
  std::cout << "\tfiber_count = " << client_config.fiber_count << '\n';
  std::cout << "\tthread_count = " << client_config.thread_count << '\n';

  EchoServiceHandlerImpl server;
  server.Run("127.0.0.1:" + std::to_string(port), server_config);

  std::atomic<bool> running{true};
  std::atomic<size_t> count{};

  EchoServiceClient client("127.0.0.1:" + std::to_string(port));

  std::vector<std::thread> threads;
  for (size_t thread_id = 0; thread_id < client_config.thread_count; ++thread_id) {
    threads.emplace_back([&]() {
      std::vector<boost::fibers::fiber> fibers;

      for (size_t fiber_id = 0; fiber_id < client_config.fiber_count; ++fiber_id) {
        fibers.emplace_back(boost::fibers::launch::post, [&]() {
          while (running) {
            EchoRequest request;
            request.set_name(std::to_string(thread_id) + ":" + std::to_string(fiber_id));
            client.SayHello(request).message();
            count.fetch_add(1);
          }
        });
      }

      for (auto& fiber : fibers) {
        fiber.join();
      }
    });
  }

  std::cout << "Warmup (5s)\n";
  std::this_thread::sleep_for(5s);  // Warm up

  std::cout << "Benchmarking (5s)\n";
  count = 0;
  auto start_time = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(5s);
  auto end_time = std::chrono::steady_clock::now();
  size_t count_result = count;

  std::cout << "Stoping clients\n";

  running = false;

  for (auto& thread : threads) {
    thread.join();
  }

  std::cout << "Stoping server\n";
  server.ShutDown();

  std::chrono::duration<double> duration = end_time - start_time;
  std::cout << "RPS: " << count_result / duration.count() << "\n\n";
}

int main() {
  RpcHandlerBase::RunConfig server_config{
      .queue_count = 1, .threads_per_queue = 1, .worker_threads_count = 1};
  ClientConfig client_config{.thread_count = 1, .fiber_count = 5000};

  BenchEchoService(server_config, client_config, 10050);
}
