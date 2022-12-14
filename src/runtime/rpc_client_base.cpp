#include "rpc_client_base.h"

#include <util/condition_check.h>

RpcClientBase::RpcClientBase(const std::string& address) noexcept
    : channel_{grpc::CreateChannel(address, grpc::InsecureChannelCredentials())},
      dispatching_thread_([this]() {
        DispatchServiceResponses();
      }) {
}

RpcClientBase::~RpcClientBase() {
  queue_.Shutdown();
  dispatching_thread_.join();
}

void RpcClientBase::DispatchServiceResponses() {
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
