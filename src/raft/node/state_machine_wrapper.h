#include <unordered_map>

#include <raft/raft.pb.h>

#include "node.h"

namespace mtf::raft::impl {

struct LastClientData {
  google::protobuf::Any response;
  uint64_t request_id = 0;
};

// Implementation of state machine with exactly-once semantics
class ExactlyOnceStateMachine {
 public:
  explicit ExactlyOnceStateMachine(IStateMachine* state_machine) noexcept;

  google::protobuf::Any Apply(const Request& request) noexcept;

 private:
  IStateMachine* state_machine_;
  std::unordered_map<uint64_t, LastClientData> clients_last_cmd_id_;
};

}  // namespace mtf::raft::impl
