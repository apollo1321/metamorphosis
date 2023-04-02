#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>

#include <rocksdb/db.h>
#include <CLI/CLI.hpp>

#include <queue/queue_service.service.h>

#include <runtime/rpc_server.h>
#include <util/condition_check.h>

using ceq::Result;
using ceq::rt::RpcError;

class QueueService final : public ceq::rt::QueueServiceStub {
 public:
  struct U64Comparator final : rocksdb::Comparator {
    int Compare(const rocksdb::Slice& a, const rocksdb::Slice& b) const {
      VERIFY(a.size() == sizeof(uint64_t) && b.size() == sizeof(uint64_t),
             "invalid size of compared keys");
      uint64_t a_val = *reinterpret_cast<const uint64_t*>(a.data());
      uint64_t b_val = *reinterpret_cast<const uint64_t*>(b.data());
      return a_val == b_val ? 0 : a_val < b_val ? -1 : +1;
    }

    const char* Name() const {
      return "U64Comparator";
    }

    // Ignore by now
    void FindShortestSeparator(std::string*, const rocksdb::Slice&) const {
    }
    void FindShortSuccessor(std::string*) const {
    }
  };

 public:
  explicit QueueService(const std::string& db_path) {
    rocksdb::Status status;

    write_options_.sync = true;

    // Open database
    {
      rocksdb::Options options;
      options.create_if_missing = true;
      options.comparator = &comparator_;
      rocksdb::DB* db{};
      VERIFY(rocksdb::DB::Open(options, db_path, &db).ok(), "cannot open database");
      database_ = std::unique_ptr<rocksdb::DB>(db);
    }

    // Read end index
    {
      std::unique_ptr<rocksdb::Iterator> iterator{database_->NewIterator(read_options_)};
      VERIFY(iterator->status().ok(), "cannot read index");
      iterator->SeekToLast();
      VERIFY(iterator->status().ok(), "cannot read index");
      if (!iterator->Valid()) {
        end_index_ = 0;
      } else {
        end_index_ = *reinterpret_cast<const uint64_t*>(iterator->key().data()) + 1;
      }
    }
  }

  Result<AppendReply, RpcError> Append(const AppendRequest& request) noexcept override {
    const uint64_t index = end_index_.fetch_add(1);
    rocksdb::Slice key(reinterpret_cast<const char*>(&index), sizeof(index));

    auto status = database_->Put(write_options_, key, request.data());
    if (!status.ok()) {
      return ceq::Err(RpcError::ErrorType::Internal, status.ToString());
    }

    AppendReply reply;
    reply.set_id(index);
    return ceq::Ok(reply);
  }

  Result<ReadReply, RpcError> Read(const ReadRequest& request) noexcept override {
    const uint64_t index = request.id();
    rocksdb::Slice key(reinterpret_cast<const char*>(&index), sizeof(index));

    std::string result;
    ReadReply reply;
    auto status = database_->Get(read_options_, key, &result);
    if (status.ok()) {
      reply.set_data(std::move(result));
      reply.set_status(ReadStatus::OK);
    } else if (status.IsNotFound()) {
      reply.set_status(ReadStatus::NO_DATA);
    } else if (!status.ok()) {
      return ceq::Err(RpcError::ErrorType::Internal, status.ToString());
    }

    return ceq::Ok(reply);
  }

  Result<google::protobuf::Empty, RpcError> Trim(const TrimRequest& request) noexcept override {
    const uint64_t start = 0;
    rocksdb::Slice start_key(reinterpret_cast<const char*>(&start), sizeof(start));

    const uint64_t end = request.id();
    rocksdb::Slice end_key(reinterpret_cast<const char*>(&end), sizeof(end));

    auto status = database_->DeleteRange(write_options_, database_->DefaultColumnFamily(),
                                         start_key, end_key);
    if (!status.ok()) {
      return ceq::Err(RpcError::ErrorType::Internal, status.ToString());
    }

    return ceq::Ok(google::protobuf::Empty{});
  }

  Result<google::protobuf::Empty, RpcError> ShutDown(
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
  const uint64_t max_queue_size_{};

  std::atomic<uint64_t> end_index_{};

  // Database
  U64Comparator comparator_;

  std::unique_ptr<rocksdb::DB> database_;

  rocksdb::WriteOptions write_options_;
  rocksdb::ReadOptions read_options_;

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

  QueueService service(db_path);

  ceq::rt::RpcServer server;
  server.Register(&service);

  server.Run(port);
  std::cout << "Running queue service at 127.0.0.1:" << port << std::endl;
  service.WaitShutDown();
  std::cout << "Shut down" << std::endl;
  server.ShutDown();
}
