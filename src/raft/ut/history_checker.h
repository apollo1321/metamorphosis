#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <runtime/time.h>

namespace ceq::raft::test {

struct RequestInfo {
  // World time timestamps
  rt::Timestamp start{};
  rt::Timestamp end{};

  uint64_t command;
  std::vector<uint64_t> result;
};

bool CheckLinearizability(std::vector<RequestInfo> history) noexcept {
  // Check that rsm append received command to it's log.
  for (auto& data : history) {
    EXPECT_EQ(data.result.back(), data.command);
    if (testing::Test::HasNonfatalFailure()) {
      return false;
    }
  }

  // The linear history of commands is the order in which a command is added to the log.
  std::sort(history.begin(), history.end(), [](auto& left, auto& right) {
    return left.result.size() < right.result.size();
  });

  // Check that all logs has the same prefix.
  for (size_t command_index = 0; command_index + 1 < history.size(); ++command_index) {
    auto& command = history[command_index];
    for (size_t index = 0; index < command.result.size(); ++index) {
      EXPECT_EQ(command.result[index], history.back().result[index]);
      if (testing::Test::HasNonfatalFailure()) {
        return false;
      }
    }
  }

  // Check that history is linearizable.
  for (size_t command_index = 0; command_index < history.size(); ++command_index) {
    auto& command = history[command_index];
    for (size_t index = 0; index < history.size(); ++index) {
      auto& other = history[index];
      if (command.end < other.start) {
        EXPECT_LT(command_index, index);
        if (testing::Test::HasNonfatalFailure()) {
          return false;
        }
      }
    }
  }

  return true;
}

}  // namespace ceq::raft::test
