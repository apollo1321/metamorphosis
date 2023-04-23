#include "rpc_generator.h"
#include "util.h"

#include <runtime/production/rpc_generator/rpc_generator.h>
#include <runtime/simulator/rpc_generator/rpc_generator.h>

using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

namespace ceq::codegen {

void GenerateClientHeader(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.h"));
  Printer printer(stream.get(), '$');

  printer.Print("#pragma once\n\n");
  printer.Print("#if defined(PRODUCTION)\n");
  printer.Print("#include \"$name$.client.prod.h\"\n", "name", file_name);
  printer.Print("#elif defined(SIMULATION)\n");
  printer.Print("#include \"$name$.client.sim.h\"\n", "name", file_name);
  printer.Print("#endif\n");
}

void GenerateServiceHeader(GeneratorContext* generator_context,
                           const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".service.h"));
  Printer printer(stream.get(), '$');

  printer.Print("#pragma once\n\n");
  printer.Print("#if defined(PRODUCTION)\n");
  printer.Print("#include \"$name$.service.prod.h\"\n", "name", file_name);
  printer.Print("#elif defined(SIMULATION)\n");
  printer.Print("#include \"$name$.service.sim.h\"\n", "name", file_name);
  printer.Print("#endif\n");
}

bool RpcGenerator::Generate(const FileDescriptor* file, const std::string& parameter,
                            GeneratorContext* generator_context, std::string* error) const {
  if (HasStreaming(file)) {
    *error = "streaming is not supported";
    return false;
  }

  prod::GenerateClientHeader(generator_context, file);
  prod::GenerateClientSource(generator_context, file);
  prod::GenerateServiceHeader(generator_context, file);
  prod::GenerateServiceSource(generator_context, file);

  sim::GenerateClientHeader(generator_context, file);
  sim::GenerateClientSource(generator_context, file);
  sim::GenerateServiceHeader(generator_context, file);
  sim::GenerateServiceSource(generator_context, file);

  GenerateClientHeader(generator_context, file);
  GenerateServiceHeader(generator_context, file);

  return true;
}

}  // namespace ceq::codegen
