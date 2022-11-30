#include <iostream>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <proto/echo_service.handler.h>
#include <proto/echo_service.pb.h>
#include <boost/fiber/all.hpp>

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

  constexpr size_t kQueueSize = 1 << 10;

  FiberTaskChannel fiber_channel(kQueueSize);

  EchoServiceHandlerImpl server;
  server.Run(argv[1], &fiber_channel, 1);

  while (!fiber_channel.is_closed()) {
    using boost::fibers::channel_op_status;
    using boost::fibers::launch;

    FiberTask* task;
    auto status = fiber_channel.pop(task);
    if (status != channel_op_status::success) {
      break;
    }

    boost::fibers::fiber(launch::post, std::ref(*task)).detach();
  }

  return 0;
}
