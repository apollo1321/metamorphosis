#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <util/result.h>

#include "errors.h"

namespace runtime_simulation {

using Address = std::string;
using Port = uint16_t;

using Duration = std::chrono::microseconds;
using Timestamp = std::chrono::time_point<Duration>;

using ServiceName = std::string;
using HandlerName = std::string;

using SerializedData = std::vector<uint8_t>;

using RpcResult = Result<SerializedData, RpcError>;

}  // namespace runtime_simulation
