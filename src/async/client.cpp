#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>

#include <proto/echo_service.grpc.pb.h>
#include <proto/echo_service.pb.h>

#include <boost/fiber/all.hpp>

class AsyncEchoService {
 public:
  explicit AsyncEchoService(const std::shared_ptr<grpc::Channel>& channel)
      : stub_{EchoService::NewStub(channel)} {
  }

  // This method is rpc service abstraction that should be called in fibers
  EchoReply SayHello(const EchoRequest& request) {
    AsyncClientCall call;

    call.response_reader = stub_->PrepareAsyncSayHello(&call.context, request, &queue_);

    call.response_reader->StartCall();
    call.response_reader->Finish(&call.reply, &call.status, static_cast<void*>(&call));

    std::unique_lock guard(call.mutex);
    call.is_ready.wait(guard, [&call]() {
      return call.is_set;
    });
    if (!call.status.ok()) {
      throw std::runtime_error("RPC Failed: " + call.status.error_message());
    }

    return std::move(call.reply);
  }

  void Run() {
    void* got_tag;
    bool ok = false;

    while (queue_.Next(&got_tag, &ok)) {
      AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);

      GPR_ASSERT(ok);

      std::unique_lock guard(call->mutex);
      call->is_ready.notify_all();

      call->is_set = true;
    }
  }

  void ShutDown() {
    queue_.Shutdown();
  }

 private:
  struct AsyncClientCall {
    EchoReply reply;
    grpc::ClientContext context;
    grpc::Status status;
    std::unique_ptr<grpc::ClientAsyncResponseReader<EchoReply>> response_reader;

    bool is_set{};
    boost::fibers::condition_variable is_ready;
    boost::fibers::mutex mutex;
  };

 private:
  grpc::CompletionQueue queue_;
  std::unique_ptr<EchoService::Stub> stub_;
};

std::string GetFiberId() {
  std::string id;
  std::stringstream ss(id);

  ss << boost::this_fiber::get_id();

  return ss.str();
}

void FiberMain(AsyncEchoService& service) {
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

  AsyncEchoService echo_service(grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials()));

  // Run service client in separate thread
  std::thread client_handler{[&echo_service]() {
    echo_service.Run();
  }};

  // Run 20 fibers on current thread
  std::vector<boost::fibers::fiber> fibers;

  for (int i = 0; i < 20; ++i) {
    fibers.emplace_back(boost::fibers::launch::post, [&echo_service]() {
      // Each fiber make 1 rpc request and prints result
      FiberMain(echo_service);
    });
  }

  for (auto& fiber : fibers) {
    fiber.join();
  }

  echo_service.ShutDown();

  client_handler.join();

  return 0;
}
