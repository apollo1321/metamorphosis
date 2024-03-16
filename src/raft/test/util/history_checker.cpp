#include "history_checker.h"

#include <algorithm>

namespace mtf::raft::test {

Status<std::string> CheckLinearizability(std::vector<RequestInfo> history) noexcept {
  // The linear history of commands is the order in which a command is added to the log.
  std::sort(history.begin(), history.end(), [](auto& left, auto& right) {
    return left.result.size() < right.result.size();
  });

  // Check that RSM append received command to it's log.
  for (const auto& data : history) {
    if (data.result.back() != data.command) {
      return Err("RSM did not append client command to log");
    }
  }

  // Check that all logs has the same prefix.
  for (size_t command_index = 0; command_index + 1 < history.size(); ++command_index) {
    auto& command = history[command_index];
    for (size_t index = 0; index < command.result.size(); ++index) {
      if (command.result[index] != history.back().result[index]) {
        return Err("Not all received logs have common prefix");
      }
    }
  }

  // Check that history is linearizable.
  for (size_t command_index = 0; command_index < history.size(); ++command_index) {
    auto& command = history[command_index];
    for (size_t index = 0; index < history.size(); ++index) {
      auto& other = history[index];
      if (command.completion_time < other.invocation_time) {
        if (command_index >= index) {
          return Err("History is not linearizable");
        }
      }
    }
  }

  return Ok();
}

}  // namespace mtf::raft::test
