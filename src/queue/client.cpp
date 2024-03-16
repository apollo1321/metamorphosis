#include <iostream>

#include <queue/queue_service.client.h>
#include <runtime/util/parse/parse.h>

#include <CLI/CLI.hpp>

using namespace mtf::rt;  // NOLINT

int main(int argc, char** argv) {
  CLI::App app{"Queue service client"};

  Endpoint address;
  app.add_option("-a,--address", address, "service ip address, addr:port")
      ->default_val("127.0.0.1:10050");

  auto shutdown = app.add_subcommand("shutdown", "shutdown service queue");

  auto store = app.add_subcommand("append", "append message in queue");
  std::string data;
  store->add_option("message", data, "message to add");

  auto read = app.add_subcommand("read", "read message from queue");
  uint64_t read_id{};
  read->add_option("id", read_id, "message id");

  auto trim = app.add_subcommand("trim", "trim queue");
  uint64_t trim_id{};
  trim->add_option("id", trim_id, "all messages in range [0, id) will be deleted");

  app.require_subcommand(1);

  CLI11_PARSE(app, argc, argv);

  rpc::QueueServiceClient client(address);

  auto handle_error = [](rpc::RpcError error) {
    std::cerr << "RPC Error: " << error.Message() << std::endl;
  };

  if (*shutdown) {
    auto result = client.ShutDown(google::protobuf::Empty{});
    if (result.HasError()) {
      handle_error(result.GetError());
    } else {
      std::cout << "shut down service" << std::endl;
    }
  }
  if (*store) {
    AppendRequest message;
    message.set_data(data);
    auto result = client.Append(message);
    if (result.HasError()) {
      handle_error(result.GetError());
    } else {
      std::cout << "append message id: " << result.GetValue().id() << std::endl;
    }
  }
  if (*read) {
    ReadRequest request;
    request.set_id(read_id);
    auto result = client.Read(request);
    if (result.HasError()) {
      handle_error(result.GetError());
    } else {
      switch (result.GetValue().status()) {
        case OK:
          std::cout << "Message: " << result.GetValue().data() << std::endl;
          break;
        case NO_DATA:
          std::cout << "No data for this id" << std::endl;
          break;
        default:
          abort();
      }
    }
  }
  if (*trim) {
    TrimRequest request;
    request.set_id(trim_id);
    auto result = client.Trim(request);
    if (result.HasError()) {
      handle_error(result.GetError());
    } else {
      std::cout << "Successfully trimmed queue" << std::endl;
    }
  }
}
