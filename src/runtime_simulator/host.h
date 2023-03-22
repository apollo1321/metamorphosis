#pragma once

#include <unordered_map>

#include <util/result.h>
#include <boost/fiber/fiber.hpp>

#include "api.h"
#include "rpc_server.h"

namespace runtime_simulation {

struct Host {
  Host(IHostRunnable* host_main, const HostOptions& options) noexcept;

  Timestamp GetLocalTime() const noexcept;
  void SleepUntil(Timestamp local_time) noexcept;

  RpcResult ProcessRequest(uint16_t port, const SerializedData& data,
                           const ServiceName& service_name,
                           const HandlerName& handler_name) noexcept;

  void RegisterServer(RpcServer* server, uint16_t port) noexcept;
  void UnregisterServer(uint16_t port) noexcept;

  ~Host();

 private:
  void RunMain(IHostRunnable* host_main) noexcept;

  Timestamp ToLocalTime(Timestamp global_time) const noexcept;
  Timestamp ToGlobalTime(Timestamp local_time) const noexcept;

 private:
  Timestamp start_time_;
  double drift_;
  Duration max_sleep_lag_;

  boost::fibers::fiber main_fiber_;

  std::unordered_map<uint16_t, RpcServer*> servers_;
};

extern Host* current_host;

using HostPtr = std::unique_ptr<Host>;

}  // namespace runtime_simulation
