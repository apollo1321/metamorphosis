#pragma once

#include <string>

#include <google/protobuf/any.pb.h>
#include <google/protobuf/message.h>

#include <util/result.h>

namespace mtf::rt::proto {

template <class T>
Result<google::protobuf::Any, std::string> ToAny(const T& proto) noexcept {
  google::protobuf::Any result;
  if (!result.PackFrom(proto)) {
    return Err("cannot convert protobuf to any");
  }
  return Ok(std::move(result));
}

template <class T>
Result<T, std::string> FromAny(const google::protobuf::Any& proto) noexcept {
  T result;
  if (!proto.UnpackTo(&result)) {
    return Err("cannot parse protobuf from any");
  }
  return Ok(std::move(result));
}

}  // namespace mtf::rt::proto
