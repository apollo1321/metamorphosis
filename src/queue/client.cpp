#include <iostream>

#include <CLI/CLI.hpp>

#include <proto/queue_service.client.h>

int main(int argc, char** argv) {
  CLI::App app{"Queue service client"};

  std::string address;
  app.add_option("-a,--address", address, "service ip address, addr:port")
      ->default_val("127.0.0.1:10050");

  auto shutdown = app.add_subcommand("shutdown", "shutdown service queue");

  auto store = app.add_subcommand("append", "append message in queue");
  std::string data;
  store->add_option("-m,--message", data, "message to add");

  auto read = app.add_subcommand("read", "read message from queue");
  uint64_t read_id{};
  read->add_option("-i,--id", read_id, "message id");

  auto trim = app.add_subcommand("trim", "trim queue");
  uint64_t trim_id{};
  trim->add_option("-i,--id", trim_id, "all messages in range [0, id) will be deleted");

  app.require_subcommand(1);

  CLI11_PARSE(app, argc, argv);

  try {
    QueueServiceClient client(address);

    if (*shutdown) {
      client.ShutDown(google::protobuf::Empty{});
      std::cout << "shutting down service" << std::endl;
    }
    if (*store) {
      AppendRequest message;
      message.set_data(data);
      auto result = client.Append(message);
      std::cout << "append message id: " << result.id() << std::endl;
    }
    if (*read) {
      ReadRequest request;
      request.set_id(read_id);
      auto result = client.Read(request);
      switch (result.status()) {
        case OK:
          std::cout << "Message: " << result.data() << std::endl;
          break;
        case NO_DATA:
          std::cout << "No data for this id" << std::endl;
          break;
        default:
          abort();
      }
    }
    if (*trim) {
      TrimRequest request;
      request.set_id(trim_id);
      auto result = client.Trim(request);
      std::cout << "Successfully trimmed queue" << std::endl;
    }
  } catch (std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }
}
