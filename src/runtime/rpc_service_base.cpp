#include "rpc_service_base.h"

#include <util/condition_check.h>

namespace runtime {

grpc::Status RpcServiceBase::SyncMethodStub() {
  VERIFY(false, "sync version of method must not be called");
  abort();
}

}  // namespace runtime
