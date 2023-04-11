#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>

#include <CLI/CLI.hpp>

#include <queue/queue_service.service.h>

#include <runtime/api.h>
#include <runtime/util/serde/string_serde.h>
#include <runtime/util/serde/u64_serde.h>

#include <util/condition_check.h>

using namespace ceq;      // NOLINT
using namespace ceq::rt;  // NOLINT

using KVStorage = kv::KVStorage<kv::U64Serde, kv::StringSerde>;

class QueueService final : public rpc::QueueServiceStub {
 public:
  explicit QueueService(KVStorage storage) noexcept : kv_storage_(std::move(storage)) {
    // Read end index
    auto iterator = kv_storage_.NewIterator();
    iterator.SeekToLast();
    if (!iterator.Valid()) {
      end_index_ = 0;
    } else {
      end_index_ = iterator.GetKey() + 1;
    }
  }

  Result<AppendReply, rpc::Error> Append(const AppendRequest& request) noexcept override {
    const uint64_t index = end_index_.fetch_add(1);
    auto result = kv_storage_.Put(index, request.data());
    if (result.HasError()) {
      return Err(rpc::Error::ErrorType::Internal, result.GetError().Message());
    }

    AppendReply reply;
    reply.set_id(index);
    return Ok(reply);
  }

  Result<ReadReply, rpc::Error> Read(const ReadRequest& request) noexcept override {
    auto result = kv_storage_.Get(request.id());
    ReadReply reply;
    if (result.HasValue()) {
      reply.set_data(result.GetValue());
      reply.set_status(ReadStatus::OK);
      return Ok(std::move(reply));
    } else if (result.GetError().error_type == db::Error::ErrorType::NotFound) {
      reply.set_status(ReadStatus::NO_DATA);
      return Ok(std::move(reply));
    } else {
      return Err(rpc::Error::ErrorType::Internal, result.GetError().Message());
    }
  }

  Result<google::protobuf::Empty, rpc::Error> Trim(const TrimRequest& request) noexcept override {
    auto result = kv_storage_.DeleteRange(0, request.id());
    if (result.HasError()) {
      return ceq::Err(rpc::Error::ErrorType::Internal, result.GetError().Message());
    }
    return ceq::Ok(google::protobuf::Empty{});
  }

  Result<google::protobuf::Empty, rpc::Error> ShutDown(
      const google::protobuf::Empty&) noexcept override {
    std::lock_guard guard(shut_down_mutex_);
    shut_down_ = true;
    shut_down_cv_.notify_one();

    return ceq::Ok(google::protobuf::Empty{});
  }

  void WaitShutDown() {
    std::unique_lock guard(shut_down_mutex_);
    shut_down_cv_.wait(guard, [this]() {
      return shut_down_;
    });
  }

 private:
  std::atomic<uint64_t> end_index_{};

  KVStorage kv_storage_;

  // ShutDown
  std::condition_variable shut_down_cv_;
  std::mutex shut_down_mutex_;

  bool shut_down_{false};
};

int main(int argc, char** argv) {
  CLI::App app{"Queue service"};

  uint16_t port;
  app.add_option("-p,--port", port, "service port")->default_val("10050");

  std::string db_path;
  app.add_option("-d,--path", db_path, "path to database directory")->default_val("/tmp/queue_db");

  CLI11_PARSE(app, argc, argv);

  db::Options options{.create_if_missing = true};

  auto db = kv::Open(db_path, options, kv::U64Serde{}, kv::StringSerde{});
  if (db.HasError()) {
    LOG_CRITICAL("Cannot open database: {}", db.GetError().Message());
    return 1;
  }

  QueueService service(std::move(db.GetValue()));

  rpc::Server server;
  server.Register(&service);

  server.Run(port);
  std::cout << "Running queue service at 127.0.0.1:" << port << std::endl;
  service.WaitShutDown();
  std::cout << "Shut down" << std::endl;
  server.ShutDown();
}
