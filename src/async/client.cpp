#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include <grpcpp/create_channel.h>

#include <proto/echo_service.client.h>

#include <boost/fiber/all.hpp>

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

  log("starting rpc call");
  EchoRequest request;
  request.set_name(GetFiberId());
  auto answer = service.SayHello(request);
  log("received answer: " + answer.message());
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "format: " << argv[0] << " addr:port" << std::endl;
    return 0;
  }
  auto channel = grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials());

  EchoServiceClient client(channel);

  // Run 20 fibers on current thread
  std::vector<boost::fibers::fiber> fibers;

  for (int i = 0; i < 20; ++i) {
    fibers.emplace_back(boost::fibers::launch::post, [&client]() {
      // Each fiber make 1 rpc request and prints result
      FiberMain(client);
    });
  }

  for (auto& fiber : fibers) {
    fiber.join();
  }

  return 0;
}
