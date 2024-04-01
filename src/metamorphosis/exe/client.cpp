#include <metamorphosis/metamorphosis.client.h>

#include <runtime/util/parse/parse.h>
#include <runtime/util/proto/conversion.h>

#include <CLI/CLI.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace mtf::rt;  // NOLINT

int main(int argc, char** argv) {
  CLI::App app{"Metamorphosis client"};

  Endpoint node;
  app.add_option("--node", node, "Metamorphosis node endpoint, addr:port")->required();

  auto append = app.add_subcommand("append", "Append message to the queue.");

  std::string message;
  append->add_option("message", message, "Message to append.")->required();

  uint64_t producer_id{};
  append->add_option("--producer-id", producer_id, "Producer id.")->required();

  uint64_t sequence_id{};
  append->add_option("--sequence-id", sequence_id, "Sequence id.")->required();

  auto read = app.add_subcommand("read", "Read message from the queue.");
  uint64_t offset{};
  read->add_option("offset", offset, "Message offset in the queue.");

  app.require_subcommand(1);

  CLI11_PARSE(app, argc, argv);

  rpc::MetamorphosisApiClient client(node);

  auto handle_error = [](rpc::RpcError error) {
    std::cerr << "RPC Error: " << error.Message() << std::endl;
  };

  if (*append) {
    AppendReq request;
    request.set_producer_id(producer_id);
    request.set_sequence_id(sequence_id);
    request.set_data(message);

    auto response_or_err = client.Append(request);
    if (response_or_err.HasError()) {
      handle_error(response_or_err.GetError());
    } else {
      auto& response = response_or_err.GetValue();
      std::cout << proto::ToString(response) << std::endl;
    }
  }
  if (*read) {
    ReadReq request;
    request.set_offset(offset);
    auto response_or_err = client.Read(request);
    if (response_or_err.HasError()) {
      handle_error(response_or_err.GetError());
    } else {
      auto& response = response_or_err.GetValue();
      std::cout << proto::ToString(response) << std::endl;
    }
  }
  return 0;
}
