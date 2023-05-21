#pragma once

#include <runtime/time.h>

#include <util/result.h>

#include <vector>

namespace ceq::raft::test {

struct RequestInfo {
  /**
   * https://jepsen.io/consistency#invocation--completion-times
   *
   * Timestampts from an imaginary, perfectly synchronized, globally accessible clock (world clock
   * from simulation).
   */
  rt::Timestamp invocation_time{};
  rt::Timestamp completion_time{};

  uint64_t command;
  std::vector<uint64_t> result;
};

/**
 * https://jepsen.io/consistency/models/linearizable
 *
 * Linearizability:
 * For any concurrent history H that the implementation generates, there is a linear history S from
 * the specification in which all commands return the same values, and if in H one command precedes
 * the other in real time, then in S these commands are in the same relative order.
 */
Status<std::string> CheckLinearizability(std::vector<RequestInfo> history) noexcept;

}  // namespace ceq::raft::test
