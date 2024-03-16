#pragma once

#include <runtime/util/endpoint/endpoint.h>

#include <boost/fiber/all.hpp>

#include <cstdint>
#include <string>

namespace mtf::rt::rpc {

class Server {
 public:
  class Service;
  class ServerImpl;

 public:
  Server() noexcept;

  void Register(Service* service) noexcept;

  // Start accepting requests
  void Start(Port port) noexcept;

  // Start processing service handlers, may be called from different threads
  void Run() noexcept;

  void ShutDown() noexcept;

  ~Server();

 private:
  ServerImpl* impl_;
};

}  // namespace mtf::rt::rpc
