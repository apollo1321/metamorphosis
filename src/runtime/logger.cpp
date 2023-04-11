#include "logger.h"

#include <google/protobuf/util/json_util.h>

std::ostream& operator<<(std::ostream& os, const google::protobuf::Message& proto) {
  std::string output;
  google::protobuf::util::JsonPrintOptions options;
  options.always_print_primitive_fields = true;
  google::protobuf::util::MessageToJsonString(proto, &output, options);
  return os << output;
}
