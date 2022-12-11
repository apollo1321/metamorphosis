#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>

#include <CLI/CLI.hpp>

#include <proto/queue.handler.h>

class QueueService final : public QueueServiceHandler {
 public:
  explicit QueueService(uint64_t max_queue_size) : max_queue_size_(max_queue_size) {
  }

  GetReply Get(const GetRequest& request) override {
    GetReply reply;

    std::lock_guard guard(queue_mutex_);
    if (request.id() >= end_index_) {
      reply.set_status(GetRequestStatus::INVALID_ID);
    } else if (request.id() < start_index_) {
      reply.set_status(GetRequestStatus::DROPPED);
    } else {
      reply.set_status(GetRequestStatus::OK);
      reply.mutable_message()->set_data(queue_.at(request.id() - start_index_));
    }
    return reply;
  }

  StoreReply Store(const Message& request) override {
    std::lock_guard guard(queue_mutex_);

    queue_.push_back(request.data());
    if (queue_.size() > max_queue_size_) {
      ++start_index_;
      queue_.pop_front();
    }

    StoreReply reply;
    reply.set_id(end_index_);
    ++end_index_;

    return reply;
  }

  google::protobuf::Empty ShutDown(const google::protobuf::Empty&) override {
    std::lock_guard guard(shut_down_mutex_);
    shut_down_ = true;
    shut_down_cv_.notify_one();

    return google::protobuf::Empty{};
  }

  void ShutDownWait() {
    std::unique_lock guard(shut_down_mutex_);
    shut_down_cv_.wait(guard, [this]() {
      return shut_down_;
    });

    QueueServiceHandler::ShutDown();
  }

 private:
  const uint64_t max_queue_size_{};

  // Queue
  std::mutex queue_mutex_;
  std::deque<std::string> queue_;
  uint64_t start_index_{};
  uint64_t end_index_{};

  // ShutDown
  std::condition_variable shut_down_cv_;
  std::mutex shut_down_mutex_;

  bool shut_down_{false};
};

int main(int argc, char** argv) {
  CLI::App app{"Queue service"};

  std::string address;
  uint64_t max_size{};
  app.add_option("-a,--address", address, "service ip address, addr:port")
      ->default_val("127.0.0.1:10050");
  app.add_option("-s,--size", max_size, "queue max size")->default_val(100);

  CLI11_PARSE(app, argc, argv);

  try {
    QueueService service(max_size);
    service.Run(address);
    std::cout << "Running queue service at " << address << std::endl;
    service.ShutDownWait();
    std::cout << "Shut down" << std::endl;
  } catch (std::exception& e) {
    std::cerr << "Service failure: " << e.what() << std::endl;
    return 1;
  }
}
