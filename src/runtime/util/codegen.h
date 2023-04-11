#pragma once

#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

namespace ceq::rt::rpc {

bool HasStreaming(const google::protobuf::FileDescriptor* file) noexcept;

std::string GetProtoFileName(const google::protobuf::FileDescriptor* file) noexcept;

std::string PackageToNamespace(const std::string& package) noexcept;

void AddMethodInfo(std::map<std::string, std::string>& vars,
                   const google::protobuf::MethodDescriptor* method);

void AddDependencyHeaders(google::protobuf::io::Printer& printer,
                          const google::protobuf::FileDescriptor* file) noexcept;

}  // namespace ceq::rt::rpc
