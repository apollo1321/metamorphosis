#include "rpc_generator.h"

#include <memory>
#include <stdexcept>

#include <codegen/util.h>

using google::protobuf::FileDescriptor;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

namespace {

void GenerateClientHeader(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  AddDependencyHeaders(printer, file);
  printer.Print("#include <runtime/simulator/rpc_client_base.h>\n\n");

  printer.Print("namespace ceq::rt {\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();

    printer.Print("\n");
    printer.Print(vars, "class $service_name$Client final : private RpcClientBase {\n");
    printer.Print("public:\n");
    printer.Indent();

    printer.Print("using RpcClientBase::RpcClientBase;\n\n");

    // Methods
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));
      printer.Print(vars,
                    "Result<$output_type$, RpcError> $method_name$(const $input_type$& input, "
                    "StopToken stop_token = {}) noexcept;\n");
    }

    printer.Outdent();
    printer.Print("};\n");
  }

  printer.Print("\n}  // namespace ceq::rt\n");
}

void GenerateClientSource(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.client.h\"\n\n", "name", file_name);

  printer.Print("namespace ceq::rt {\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);

    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Client";

    // Methods
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));

      printer.Print("\n");
      printer.Print(vars,
                    "Result<$output_type$, RpcError> $service_class$::$method_name$(const "
                    "$input_type$& input, StopToken stop_token) noexcept {\n");
      printer.Indent();
      printer.Print(vars,
                    "return MakeRequest<$input_type$, $output_type$>(input, "
                    "\"$service_name$\", \"$method_name$\", std::move(stop_token));\n");
      printer.Outdent();
      printer.Print("}\n");
    }
  }

  printer.Print("\n}  // namespace ceq::rt\n");
}

void GenerateServiceHeader(GeneratorContext* generator_context, const FileDescriptor* file) {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".service.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  AddDependencyHeaders(printer, file);
  printer.Print("#include <runtime/simulator/rpc_service_base.h>\n\n");

  printer.Print("namespace ceq::rt {\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Stub";

    printer.Print("\n");
    printer.Print(vars, "class $service_class$ : public RpcServer::RpcService {\n");
    printer.Print("public:\n");
    printer.Indent();

    // Constructor
    printer.Print(vars, "$service_class$() noexcept;\n\n");

    // Placeholders for services
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));

      printer.Print(vars,
                    "virtual Result<$output_type$, RpcError> $method_name$(const $input_type$& "
                    "request) noexcept = 0;\n");
    }
    printer.Print("\n");

    printer.Outdent();
    printer.Print("private:\n");
    printer.Indent();

    printer.Print(
        vars,
        "RpcResult ProcessRequest(const SerializedData& data, const HandlerName& handler_name) "
        "noexcept override;\n");

    printer.Outdent();
    printer.Print("};\n");
  }

  printer.Print("\n}  // namespace ceq::rt\n");
}

void GenerateServiceSource(GeneratorContext* generator_context, const FileDescriptor* file) {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".service.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.service.h\"\n\n", "name", file_name);

  printer.Print("namespace ceq::rt {\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Stub";

    printer.Print("\n");

    // Constructor
    printer.Print(vars,
                  "$service_class$::$service_class$() noexcept : "
                  "RpcServer::RpcService{\"$service_name$\"} {}\n\n");

    printer.Print(
        vars,
        "RpcResult $service_class$::ProcessRequest(const SerializedData& data, const HandlerName& "
        "handler_name) noexcept {\n");
    printer.Indent();
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));

      printer.Print(vars, "if (handler_name == \"$method_name$\") {\n");
      printer.Indent();
      printer.Print(vars,
                    "return ProcessRequestWrapper<$input_type$, $output_type$>(data, [&](const "
                    "auto& request){ return $method_name$(request); });\n");
      printer.Outdent();
      printer.Print(vars, "}\n");
    }
    printer.Print(vars, "return Err(RpcError::ErrorType::HandlerNotFound);\n");
    printer.Outdent();
    printer.Print("}\n");
  }

  printer.Print("\n}  // namespace ceq::rt\n");
}

}  // namespace

bool RpcGenerator::Generate(const FileDescriptor* file, const std::string& parameter,
                            GeneratorContext* generator_context, std::string* error) const {
  if (HasStreaming(file)) {
    *error = "streaming is not supported";
    return false;
  }

  GenerateClientHeader(generator_context, file);
  GenerateClientSource(generator_context, file);

  GenerateServiceHeader(generator_context, file);
  GenerateServiceSource(generator_context, file);

  return true;
}
