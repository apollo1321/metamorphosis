#include "rpc_service_base.h"

#include <util/condition_check.h>

namespace ceq::rt {

grpc::Status RpcServer::RpcService::SyncMethodStub() {
  VERIFY(false, "sync version of method must not be called");
  abort();
}

}  // namespace ceq::rt
