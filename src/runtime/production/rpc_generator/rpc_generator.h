#pragma once

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>

using google::protobuf::FileDescriptor;
using google::protobuf::compiler::GeneratorContext;

namespace mtf::codegen::prod {

void GenerateClientHeader(GeneratorContext* generator_context, const FileDescriptor* file) noexcept;
void GenerateClientSource(GeneratorContext* generator_context, const FileDescriptor* file) noexcept;
void GenerateServiceHeader(GeneratorContext* generator_context,
                           const FileDescriptor* file) noexcept;
void GenerateServiceSource(GeneratorContext* generator_context,
                           const FileDescriptor* file) noexcept;

}  // namespace mtf::codegen::prod
