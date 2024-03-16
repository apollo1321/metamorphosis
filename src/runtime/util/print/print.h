#pragma once

#include <google/protobuf/message.h>

#include <runtime/util/chrono/chrono.h>

namespace mtf::rt {

std::string ToString(const Duration& duration) noexcept;
std::string ToString(const google::protobuf::Message& proto) noexcept;

}  // namespace mtf::rt
