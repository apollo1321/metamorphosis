#include <iostream>

#include <CLI/CLI.hpp>

#include <proto/queue.client.h>

int main(int argc, char** argv) {
  CLI::App app{"Queue service client"};

  std::string address;
  app.add_option("-a,--address", address, "service ip address, addr:port")
      ->default_val("127.0.0.1:10050");

  auto shutdown = app.add_subcommand("shutdown", "shutdown service queue");

  auto store = app.add_subcommand("store", "store message in queue");
  std::string data;
  store->add_option("-m,--message", data, "message to add");

  auto get = app.add_subcommand("get", "get message from queue");
  uint64_t id{};
  get->add_option("-i,--id", id, "message id");

  app.require_subcommand(1);

  CLI11_PARSE(app, argc, argv);

  try {
    QueueServiceClient client(address);

    if (*shutdown) {
      client.ShutDown(google::protobuf::Empty{});
      std::cout << "shutting down service" << std::endl;
    }
    if (*store) {
      Message message;
      message.set_data(data);
      auto result = client.Store(message);
      std::cout << "stored message id: " << result.id() << std::endl;
    }
    if (*get) {
      GetRequest request;
      request.set_id(id);
      auto result = client.Get(request);
      switch (result.status()) {
        case OK:
          std::cout << "Message: " << result.message().data() << std::endl;
          break;
        case INVALID_ID:
          std::cout << "Invalid id" << std::endl;
          break;
        case DROPPED:
          std::cout << "Message was dropped" << std::endl;
          break;
        default:
          std::cout << "unexpected result" << std::endl;
      }
    }
  } catch (std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }
}
