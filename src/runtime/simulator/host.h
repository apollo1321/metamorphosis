#pragma once

#include <map>
#include <unordered_map>

#include <boost/fiber/fiber.hpp>

#include <runtime/database.h>
#include <runtime/logger.h>
#include <util/result.h>

#include "api.h"
#include "database.h"
#include "rpc_server.h"

namespace ceq::rt::sim {

class Host {
 public:
  Host(const Address& address, IHostRunnable* host_main, const HostOptions& options) noexcept;

  Timestamp GetLocalTime() const noexcept;
  bool SleepUntil(Timestamp local_time, StopToken stop_token = StopToken{}) noexcept;

  Result<rpc::SerializedData, rpc::RpcError> ProcessRequest(
      uint16_t port, const rpc::SerializedData& data, const rpc::ServiceName& service_name,
      const rpc::HandlerName& handler_name) noexcept;

  Result<rpc::SerializedData, rpc::RpcError> MakeRequest(const Endpoint& endpoint,
                                                         const rpc::SerializedData& data,
                                                         const rpc::ServiceName& service_name,
                                                         const rpc::HandlerName& handler_name,
                                                         StopToken stop_token) noexcept;

  void RegisterServer(rpc::Server::ServerImpl* server, uint16_t port) noexcept;
  void UnregisterServer(uint16_t port) noexcept;

  std::shared_ptr<spdlog::logger> GetLogger() noexcept;

  void PauseHost() noexcept;
  void ResumeHost() noexcept;

  void KillHost() noexcept;
  void StartHost() noexcept;

  size_t GetCurrentEpoch() const noexcept;

  void StopFiberIfNecessary() noexcept;

  ~Host();

 public:
  std::map<std::string, db::HostDatabase> databases;

 private:
  void RunMain() noexcept;

  Timestamp ToLocalTime(Timestamp global_time) const noexcept;
  Timestamp ToGlobalTime(Timestamp local_time) const noexcept;

 private:
  Timestamp start_time_;
  double drift_;
  Duration max_sleep_lag_;

  IHostRunnable* host_main_;
  boost::fibers::fiber main_fiber_;

  Address address_;

  std::unordered_map<uint16_t, rpc::Server::ServerImpl*> servers_;

  std::shared_ptr<spdlog::logger> logger_;

  bool paused_ = false;
  boost::fibers::mutex pause_lk_;
  boost::fibers::condition_variable pause_cv_;

  size_t epoch_ = 0;
};

Host* GetCurrentHost() noexcept;
size_t GetCurrentEpoch() noexcept;

using HostPtr = std::unique_ptr<Host>;

}  // namespace ceq::rt::sim
