#include "logger.h"

#include <runtime/util/print/print.h>

std::ostream& operator<<(std::ostream& os, const google::protobuf::Message& proto) {
  return os << mtf::rt::ToString(proto);
}
