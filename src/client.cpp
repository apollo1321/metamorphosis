#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>

#include <grpcpp/create_channel.h>
#include <proto/echo_service.client.h>
#include <boost/fiber/all.hpp>

std::atomic<size_t> rpc_call_count;
std::chrono::steady_clock::time_point start;

std::string GetFiberId() {
  std::string id;
  std::stringstream ss(id);

  ss << boost::this_fiber::get_id();

  return ss.str();
}

void FiberMain(EchoServiceClient& service) {
  auto log = [](const auto& message) {
    std::cout << "[fiber " << GetFiberId() << "]: " << message << std::endl;
  };

  EchoRequest request;
  request.set_name(GetFiberId());
  try {
    auto answer = service.SayHello(request);
  } catch (std::exception& e) {
    log("received error: " + std::string(e.what()));
  }
  auto val = rpc_call_count.fetch_add(1);
  if (val == 0) {
    start = std::chrono::steady_clock::now();
  }
  if (val % 5000 == 0) {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> duration = now - start;
    std::cout << val << " : " << val / duration.count() << " rps" << std::endl;
  }
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "format: " << argv[0] << " addr:port" << std::endl;
    return 0;
  }

  std::vector<std::thread> threads;
  for (int thread_id = 0; thread_id < 1; ++thread_id) {
    threads.emplace_back([&]() {
      auto channel = grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials());

      EchoServiceClient client(channel);

      std::vector<boost::fibers::fiber> fibers;

      for (int fiber_id = 0; fiber_id < 5000; ++fiber_id) {
        fibers.emplace_back(boost::fibers::launch::post, [&client]() {
          while (true) {
            FiberMain(client);
          }
        });
      }

      for (auto& fiber : fibers) {
        fiber.join();
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  return 0;
}
