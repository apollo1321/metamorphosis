#include "state_machine_wrapper.h"

namespace mtf::raft::impl {

ExactlyOnceStateMachine::ExactlyOnceStateMachine(IStateMachine* state_machine) noexcept
    : state_machine_{state_machine} {
}

google::protobuf::Any ExactlyOnceStateMachine::Apply(const Request& request) noexcept {
  auto it = clients_last_cmd_id_.find(request.client_id());
  if (it == clients_last_cmd_id_.end()) {
    it = clients_last_cmd_id_.emplace(request.client_id(), LastClientData{}).first;
  }

  auto& last_data = it->second;

  if (last_data.request_id == request.request_id()) {
    return last_data.response;
  }
  last_data.response = state_machine_->Apply(request.command());
  last_data.request_id = request.request_id();

  return last_data.response;
}

}  // namespace mtf::raft::impl
