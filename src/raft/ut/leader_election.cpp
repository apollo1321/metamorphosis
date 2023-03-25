#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>

#include <runtime/simulator/api.h>

TEST(RaftElection, SimplyWorks) {
  struct Host final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
      ceq::rt::RpcServer server;
    }
  };

  struct Client final : public ceq::rt::IHostRunnable {
    void Main() noexcept override {
    }
  };

  Host host;
  Client client;

  ceq::rt::InitWorld(42);
  ceq::rt::AddHost("addr1", &host);
  ceq::rt::AddHost("addr2", &client);
  ceq::rt::RunSimulation();
}
