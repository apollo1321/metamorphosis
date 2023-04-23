#pragma once

#include <google/protobuf/message.h>

#include <runtime/util/chrono/chrono.h>

namespace ceq::rt {

std::string ToString(const Duration& duration) noexcept;
std::string ToString(const google::protobuf::Message& proto) noexcept;

}  // namespace ceq::rt
