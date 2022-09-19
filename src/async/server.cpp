#include <iostream>
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>

#include <proto/echo_service.grpc.pb.h>
#include <proto/echo_service.pb.h>

#include <boost/fiber/all.hpp>

using FiberTask = std::function<void()>;
using FiberTaskChannel = boost::fibers::buffered_channel<FiberTask>;

// This function is actual rpc service implementation running in fiber
EchoReply SayHello(const EchoRequest& request) {
  EchoReply reply;
  reply.set_message("Hello from async server " + request.name());
  return reply;
}

class AsyncEchoService final {
 public:
  void Run(std::string address, FiberTaskChannel& fiber_channel) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    queue_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();

    std::cout << "AsyncEchoService: listening on " << address << std::endl;

    AddCallData();

    void* tag;
    bool ok;
    while (queue_->Next(&tag, &ok)) {
      GPR_ASSERT(ok);
      auto call_data = static_cast<CallData*>(tag);
      if (call_data->status == CallData::PROCESS) {
        AddCallData();

        fiber_channel.push([call_data]() {
          auto reply = SayHello(call_data->request);
          call_data->status = CallData::FINISH;
          call_data->responder.Finish(std::move(reply), grpc::Status::OK, call_data);
        });

      } else {
        GPR_ASSERT(call_data->status == CallData::FINISH);
        delete call_data;
      }
    }
  }

 private:
  struct CallData {
    CallData() : responder(&ctx), status(CallStatus::PROCESS) {
    }

    grpc::ServerContext ctx;
    grpc::ServerAsyncResponseWriter<EchoReply> responder;

    EchoRequest request;

    enum CallStatus { PROCESS, FINISH };
    CallStatus status;
  };

 private:
  void AddCallData() {
    auto call_data = new CallData;
    service_.RequestSayHello(&call_data->ctx, &call_data->request, &call_data->responder,
                             queue_.get(), queue_.get(), call_data);
  }

 private:
  std::unique_ptr<grpc::ServerCompletionQueue> queue_;
  EchoService::AsyncService service_;
  std::unique_ptr<grpc::Server> server_;
};

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "format: " << argv[0] << " addr:port" << std::endl;
    return 0;
  }

  constexpr size_t kQueueSize = 1 << 10;

  FiberTaskChannel fiber_channel(kQueueSize);

  AsyncEchoService service;

  std::thread service_thread([&]() {
    service.Run(argv[1], fiber_channel);
  });

  while (!fiber_channel.is_closed()) {
    using boost::fibers::channel_op_status;
    using boost::fibers::launch;

    FiberTask task;
    auto status = fiber_channel.pop(task);
    if (status != channel_op_status::success) {
      break;
    }

    std::cerr << "Start executing task\n";

    boost::fibers::fiber(launch::post, std::move(task)).detach();
  }

  service_thread.join();

  return 0;
}
