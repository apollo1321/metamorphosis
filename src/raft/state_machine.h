#include <unordered_map>

#include <raft/raft.pb.h>

#include "node.h"

namespace ceq::raft::impl {

struct LastClientData {
  google::protobuf::Any response;
  uint64_t request_id = 0;
};

// Implementation of state machine with exactly-once semantics
class StateMachine {
 public:
  explicit StateMachine(IStateMachine* state_machine) noexcept;

  google::protobuf::Any Execute(const Request& request) noexcept;

 private:
  IStateMachine* state_machine_;
  std::unordered_map<uint64_t, LastClientData> clients_last_cmd_id_;
};

}  // namespace ceq::raft::impl
