#include <iostream>

#include <grpcpp/grpcpp.h>

#include <proto/messages.pb.h>
#include <proto/messages.grpc.pb.h>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "format: " << argv[0] << " addr:port" << std::endl;
    return 0;
  }

  auto channel = grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials()); 

  HelloRequest request;
  request.set_name("Hello");

  HelloReply reply;

  auto stub = EchoService::NewStub(channel);
  grpc::ClientContext context;

  auto status = stub->SayHello(&context, request, &reply);
  if (status.ok()) {
    std::cout << reply.message() << std::endl;
  } else {
    std::cout << "Error: " << status.error_message() << std::endl;
  }

  return 0;
}
