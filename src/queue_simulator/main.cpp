#include <iostream>
#include <vector>

#include <runtime_simulator/world.h>

#include <proto/replicated_queue.pb.h>

class QueueServiceClient final {
 public:
  explicit QueueServiceClient(std::string address);

  const std::string& Address() noexcept;

  ReadReply Read(const ReadRequest& input);
  AppendReply Append(const AppendRequest& input);
};

class QueueServiceStub : public runtime_simulation::RpcServiceBase {
 public:
  virtual AppendReply Append(const AppendRequest& request) = 0;
  virtual ReadReply Read(const ReadRequest& request) = 0;
};

struct Server : public runtime_simulation::Server, public QueueServiceStub {
  explicit Server(std::string address) : runtime_simulation::Server{address} {
    Register(static_cast<QueueServiceStub*>(this));
  }

  void RunMain() override {
  }

  AppendReply Append(const AppendRequest& request) override {
    data.emplace_back(request.data());
    AppendReply reply;
    reply.set_id(data.size());
    return reply;
  }

  ReadReply Read(const ReadRequest& request) override {
    ReadReply reply;
    if (data.size() < request.id()) {
      reply.set_status(ReadStatus::NO_DATA);
    } else {
      reply.set_status(ReadStatus::OK);
      reply.set_data(data[request.id()]);
    }
    return reply;
  }

  std::vector<std::string> data;
};

struct Client : public runtime_simulation::IClient {
  explicit Client(std::string server_addr) : client(server_addr) {
  }

  void RunMain() override {
    AppendRequest append_request;
    append_request.set_data("request_to_" + client.Address());
    auto append_result = client.Append(append_request);
    std::cerr << append_result.id() << std::endl;

    ReadRequest read_request;
    read_request.set_id(append_result.id());
    auto read_reply = client.Read(read_request);
    std::cerr << read_reply.data() << std::endl;
  }

  QueueServiceClient client;
};

int main() {
  Server server1("server1");
  Server server2("server2");
  Server server3("server3");

  auto world = runtime_simulation::GetWorld();

  world->AddServer(&server1);
  world->AddServer(&server2);
  world->AddServer(&server3);

  Client client1("server1");
  Client client2("server2");
  Client client3("server3");

  world->AddClient(&client1);
  world->AddClient(&client2);
  world->AddClient(&client3);

  world->RunSimulation();

  return 0;
}
