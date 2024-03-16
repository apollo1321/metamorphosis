#pragma once

#include <raft/client/client.h>
#include <raft/node/node.h>
#include <raft/raft.client.h>
#include <raft/raft.pb.h>

#include <runtime/logger.h>
#include <runtime/simulator/api.h>

#include <raft/test/util/history_checker.h>
#include <raft/test/util/logging_state_machine.h>
#include <raft/test/util/logging_state_machine.pb.h>

#include <vector>

namespace mtf::raft::test {

struct RaftHost final : public rt::sim::IHostRunnable {
  explicit RaftHost(raft::RaftConfig config) noexcept : config{std::move(config)} {
  }

  void Main() noexcept override {
    LoggingStateMachine state_machine;
    raft::RunMain(&state_machine, config).ExpectOk();
  }

  raft::RaftConfig config;
};

struct RaftClientHost final : public rt::sim::IHostRunnable {
  explicit RaftClientHost(const std::vector<rt::Endpoint>& raft_nodes,
                          std::vector<RequestInfo>& history,
                          RaftClient::Config client_config) noexcept
      : raft_nodes{raft_nodes}, history{history}, client_config{client_config} {
  }

  void Main() noexcept override {
    raft::RaftClient client(raft_nodes);

    while (true) {
      RsmCommand command;
      command.set_data(std::uniform_int_distribution<uint8_t>()(rt::GetGenerator()));

      LOG("CLIENT: write command: {}", command.data());

      RequestInfo info;
      info.command = command.data();
      info.invocation_time = rt::sim::GetGlobalTime();
      auto response = client.Apply<RsmResult>(command, client_config);
      if (response.HasError()) {
        LOG("CLIENT: request error: {}", response.GetError().Message());
      } else {
        auto& value = response.GetValue();
        LOG("CLIENT: response = {}", value);

        info.completion_time = rt::sim::GetGlobalTime();
        info.result.resize(value.log_entries_size());
        std::copy(value.log_entries().begin(), value.log_entries().end(), info.result.begin());
        history.emplace_back(std::move(info));
      }
    }
  }

  std::vector<rt::Endpoint> raft_nodes;
  std::vector<RequestInfo>& history;
  RaftClient::Config client_config;
};

struct CrashSupervisor final : public rt::sim::IHostRunnable {
  explicit CrashSupervisor(const std::vector<rt::Endpoint>& raft_nodes,
                           rt::Duration max_pause_time) noexcept
      : raft_nodes{raft_nodes}, max_pause_time{max_pause_time} {
  }

  void Main() noexcept override {
    while (true) {
      auto host = GetRandomHost();
      rt::Duration pause_time = GetPauseTime();
      LOG("Kill host {}", host);
      rt::sim::KillHost(host);
      rt::SleepFor(pause_time);
      LOG("Start host {}", host);
      rt::sim::StartHost(host);
    }
  }

  rt::Address GetRandomHost() noexcept {
    size_t index =
        std::uniform_int_distribution<size_t>(0, raft_nodes.size() - 1)(rt::GetGenerator());
    return raft_nodes[index].address;
  }

  rt::Duration GetPauseTime() noexcept {
    return rt::Duration{std::uniform_int_distribution<rt::Duration::rep>(
        0, max_pause_time.count())(rt::GetGenerator())};
  }

  std::vector<rt::Endpoint> raft_nodes;
  rt::Duration max_pause_time;
};

}  // namespace mtf::raft::test
