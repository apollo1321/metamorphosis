#include <chrono>
#include <iostream>
#include <thread>

#include <proto/echo_service.handler.h>
#include <proto/echo_service.pb.h>

using namespace std::chrono_literals;

class EchoServiceHandlerImpl final : public EchoServiceHandler {
  EchoReply SayHello(const EchoRequest& request) override {
    EchoReply reply;
    reply.set_message("Hello from async server " + request.name());
    return reply;
  }

  EchoReply SayHello2(const EchoRequest& request) override {
    EchoReply reply;
    reply.set_message("Hello from async server 2 " + request.name());
    return reply;
  }
};

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "format: " << argv[0] << " addr:port" << std::endl;
    return 0;
  }

  EchoServiceHandlerImpl server;
  RpcHandlerBase::RunConfig config{
      .worker_threads_count = 1, .queue_count = 1, .threads_per_queue = 1};
  server.Run(argv[1], config);
  std::this_thread::sleep_for(500h);

  return 0;
}
