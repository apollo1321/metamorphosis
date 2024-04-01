#include "logger.h"

#include <runtime/util/proto/conversion.h>

std::ostream& operator<<(std::ostream& os, const google::protobuf::Message& proto) {
  return os << mtf::rt::proto::ToString(proto);
}
