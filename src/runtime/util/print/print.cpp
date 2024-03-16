#include "print.h"

#include <util/condition_check.h>

#include <google/protobuf/util/json_util.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace mtf::rt {

std::string ToString(const Duration& duration) noexcept {
  auto us = duration.count() % 1000;
  auto ms = duration.count() / 1000 % 1000;
  auto sec = duration.count() / 1000 / 1000 % 60;
  auto min = duration.count() / 1000 / 1000 / 60 % 60;
  auto hours = duration.count() / 1000 / 1000 / 60 / 60;

  std::ostringstream os;

  os << '[' << std::setfill('0')      //
     << std::setw(2) << hours << ':'  //
     << std::setw(2) << min << ':'    //
     << std::setw(2) << sec << '.'    //
     << std::setw(3) << ms << '.'     //
     << std::setw(3) << us            //
     << ']';

  return os.str();
}

std::string ToString(const google::protobuf::Message& proto) noexcept {
  std::string output;
  google::protobuf::util::JsonPrintOptions options;
  options.always_print_primitive_fields = true;
  VERIFY(google::protobuf::util::MessageToJsonString(proto, &output, options).ok(),
         "protobuf serialization error");
  return output;
}

}  // namespace mtf::rt
