#include "rpc_generator.h"

#include <memory>
#include <stdexcept>

#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

using google::protobuf::FileDescriptor;
using google::protobuf::compiler::CodeGenerator;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

namespace {

bool HasStreaming(const FileDescriptor* file) noexcept {
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    auto service = file->service(service_id);
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      auto method = service->method(method_id);
      if (method->server_streaming() || method->client_streaming()) {
        return true;
      }
    }
  }
  return false;
}

std::string GetProtoFileName(const FileDescriptor* file) noexcept {
  return file->name().substr(0, file->name().size() + 1 - sizeof(".proto"));
}

void GenerateClientHeader(const FileDescriptor* file,
                          GeneratorContext* generator_context) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  printer.Print("#include <rpc_client_base.h>\n\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();

    printer.Print(vars, "class $service_name$Client final : public RpcClientBase {\n");
    printer.Print("public:\n");
    printer.Indent();

    printer.Print("using RpcClientBase::RpcClientBase;\n\n");

    // Methods
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      auto method = service->method(method_id);
      vars["method_name"] = method->name();
      vars["output_type"] = method->output_type()->name();
      vars["input_type"] = method->input_type()->name();
      printer.Print(vars, "$output_type$ $method_name$(const $input_type$& input);\n");
    }

    printer.Outdent();
    printer.Print("};\n\n");
  }
}

void GenerateClientSource(const FileDescriptor* file,
                          GeneratorContext* generator_context) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.client.h\"\n\n", "name", file_name);

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);

    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Client";

    // Methods
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      auto method = service->method(method_id);
      vars["method_name"] = method->name();
      vars["output_type"] = method->output_type()->name();
      vars["input_type"] = method->input_type()->name();
      printer.Print(vars,
                    "$output_type$ $service_class$::$method_name$(const $input_type$& input) {\n");
      printer.Indent();
      printer.Print(vars,
                    "return MethodImpl<$input_type$, $output_type$>(input, \"$method_name$\");\n");
      printer.Outdent();
      printer.Print("}\n");
    }
  }
}

/* void GenerateHandlerHeader(const FileDescriptor* file, Printer& printer) { */
/* } */

/* void GenerateHandlerSource(const FileDescriptor* file, Printer& printer) { */
/* } */

}  // namespace

bool RpcGenerator::Generate(const FileDescriptor* file, const std::string& parameter,
                            GeneratorContext* generator_context, std::string* error) const {
  if (HasStreaming(file)) {
    *error = "streaming is not supported";
    return false;
  }

  GenerateClientHeader(file, generator_context);
  GenerateClientSource(file, generator_context);

  return true;
}
