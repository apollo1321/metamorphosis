#include <raft/test/util/history_checker.h>

#include <runtime/util/parse/parse.h>

#include <spdlog/spdlog.h>
#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace std::chrono_literals;
using namespace mtf::rt;  // NOLINT
using mtf::raft::test::RequestInfo;

std::vector<RequestInfo> ParseHistory(std::istream& is) noexcept {
  std::vector<RequestInfo> result;

  while (is) {
    std::string tag;
    is >> tag;
    if (tag != "OK:") {
      std::string line;
      std::getline(is, line);
      continue;
    }

    uint64_t invocation_time{}, completion_time{};
    is >> invocation_time >> completion_time;

    uint64_t command{};
    is >> command;

    char bracket{};
    is >> bracket;
    if (bracket != '[') {
      break;
    }

    size_t count{};
    is >> count;
    std::vector<uint64_t> log;
    while (count != 0 && is) {
      uint64_t data{};
      is >> data;

      log.emplace_back(data);
      --count;
    }

    is >> bracket;
    if (bracket != ']') {
      break;
    }

    Timestamp ts;

    result.emplace_back(RequestInfo{
        .invocation_time = Timestamp(Duration(invocation_time)),
        .completion_time = Timestamp(Duration(completion_time)),
        .command = command,
        .result = std::move(log),
    });
  }

  return result;
}

int main(int argc, char** argv) {
  CLI::App app{"Raft history checker"};

  std::filesystem::path path;
  app.add_option("path", path, "Path to history file")->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);

  auto history = ParseHistory(std::cin);
  fmt::print("Successes: {}\n", history.size());
  auto result = mtf::raft::test::CheckLinearizability(history);
  if (result.HasError()) {
    fmt::print("Fail: {}\n", result.GetError());
    return 1;
  }
  fmt::print("Ok\n");

  return 0;
}
