#include "rpc_service_base.h"

namespace mtf::rt::rpc {

Server::Service::Service(ServiceName service_name) noexcept
    : service_name_{std::move(service_name)} {
}

const ServiceName& Server::Service::GetServiceName() noexcept {
  return service_name_;
}

}  // namespace mtf::rt::rpc
