#pragma once

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>

namespace mtf::codegen {

using google::protobuf::FileDescriptor;
using google::protobuf::compiler::GeneratorContext;

class RpcGenerator : public google::protobuf::compiler::CodeGenerator {
 public:
  bool Generate(const FileDescriptor* file, const std::string& parameter,
                GeneratorContext* generator_context, std::string* error) const override;
};

}  // namespace mtf::codegen
