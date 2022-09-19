#include <iostream>

#include <grpcpp/grpcpp.h>

#include <proto/echo_service.grpc.pb.h>
#include <proto/echo_service.pb.h>

class ServiceImpl : public EchoService::Service {
  grpc::Status SayHello(grpc::ServerContext*, const EchoRequest* request,
                        EchoReply* reply) override {
    reply->set_message("Server: " + request->name());
    return grpc::Status::OK;
  }
};

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "format: " << argv[0] << " addr:port" << std::endl;
    return 0;
  }

  ServiceImpl service;
  grpc::ServerBuilder builder;
  builder.AddListeningPort(argv[1], grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  std::cout << "Listening at " << argv[1] << std::endl;

  server->Wait();

  return 0;
}
