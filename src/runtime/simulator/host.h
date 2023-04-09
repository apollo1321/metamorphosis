#pragma once

#include <unordered_map>

#include <util/result.h>
#include <boost/fiber/fiber.hpp>

#include "api.h"
#include "rpc_server.h"

namespace ceq::rt {

struct Host {
  Host(const Address& address, IHostRunnable* host_main, const HostOptions& options) noexcept;

  Timestamp GetLocalTime() const noexcept;
  bool SleepUntil(Timestamp local_time, StopToken stop_token = StopToken{}) noexcept;

  RpcResult ProcessRequest(uint16_t port, const SerializedData& data,
                           const ServiceName& service_name,
                           const HandlerName& handler_name) noexcept;

  RpcResult MakeRequest(const Endpoint& endpoint, const SerializedData& data,
                        const ServiceName& service_name, const HandlerName& handler_name,
                        StopToken stop_token) noexcept;

  void RegisterServer(RpcServer::RpcServerImpl* server, uint16_t port) noexcept;
  void UnregisterServer(uint16_t port) noexcept;

  std::shared_ptr<spdlog::logger> GetLogger() noexcept;

  void PauseHost() noexcept;
  void ResumeHost() noexcept;

  void KillHost() noexcept;
  void StartHost() noexcept;

  size_t GetCurrentEpoch() const noexcept;

  ~Host();

 private:
  void RunMain() noexcept;

  Timestamp ToLocalTime(Timestamp global_time) const noexcept;
  Timestamp ToGlobalTime(Timestamp local_time) const noexcept;

  void WaitIfPaused() noexcept;
  void LockForeverIfOldEpoch() const noexcept;

 private:
  Timestamp start_time_;
  double drift_;
  Duration max_sleep_lag_;

  IHostRunnable* host_main_;
  boost::fibers::fiber main_fiber_;

  std::unordered_map<uint16_t, RpcServer::RpcServerImpl*> servers_;

  std::shared_ptr<spdlog::logger> logger_;

  bool paused_ = false;
  boost::fibers::mutex pause_lk_;
  boost::fibers::condition_variable pause_cv_;

  size_t epoch_ = 0;
};

Host* GetCurrentHost() noexcept;
size_t GetCurrentEpoch() noexcept;

using HostPtr = std::unique_ptr<Host>;

}  // namespace ceq::rt
