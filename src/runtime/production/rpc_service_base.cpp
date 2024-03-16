#include "rpc_service_base.h"

#include <util/condition_check.h>

namespace mtf::rt::rpc {

grpc::Status Server::Service::SyncMethodStub() {
  VERIFY(false, "sync version of method must not be called");
  abort();
}

}  // namespace mtf::rt::rpc
