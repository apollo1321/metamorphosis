#include "rpc_client_base.h"

#include <util/condition_check.h>

namespace mtf::rt::rpc {

ClientBase::ClientBase(const Endpoint& endpoint) noexcept
    : channel_{grpc::CreateChannel(endpoint.ToString(), grpc::InsecureChannelCredentials())},
      dispatching_thread_([this]() {
        DispatchServiceResponses();
      }) {
}

ClientBase::~ClientBase() {
  queue_.Shutdown();
  dispatching_thread_.join();
}

void ClientBase::DispatchServiceResponses() {
  void* got_tag = nullptr;
  bool ok = false;

  while (queue_.Next(&got_tag, &ok)) {
    EventTrigger* call = static_cast<EventTrigger*>(got_tag);

    VERIFY(ok, "ok should always be true");

    std::unique_lock guard(call->mutex);
    call->is_ready.notify_all();
    call->is_set = true;
  }
}

}  // namespace mtf::rt::rpc
