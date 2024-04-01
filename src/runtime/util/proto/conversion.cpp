#include "conversion.h"

#include <util/condition_check.h>

#include <google/protobuf/util/json_util.h>

#include <string>

namespace mtf::rt::proto {

std::string ToString(const google::protobuf::Message& proto) noexcept {
  std::string output;
  google::protobuf::util::JsonPrintOptions options;
  options.always_print_primitive_fields = true;
  VERIFY(google::protobuf::util::MessageToJsonString(proto, &output, options).ok(),
         "protobuf serialization error");
  return output;
}

}  // namespace mtf::rt
