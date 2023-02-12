#include "rpc_service_base.h"

#include <util/condition_check.h>

grpc::Status RpcServiceBase::SyncMethodStub() {
  VERIFY(false, "sync version of method must not be called");
  abort();
}
