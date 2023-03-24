#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <util/result.h>

#include <runtime/rpc_error.h>

namespace ceq::rt {

using ServiceName = std::string;
using HandlerName = std::string;

using SerializedData = std::vector<uint8_t>;

using RpcResult = Result<SerializedData, RpcError>;

}  // namespace ceq::rt::impl
