#include "rpc_generator.h"

#include <map>
#include <memory>
#include <string>

#include <runtime/rpc_generator/util.h>

using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

namespace ceq::codegen::sim {

void GenerateClientHeader(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(
      generator_context->Open(file_name + ".client.sim.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  AddDependencyHeaders(printer, file);
  printer.Print("#include <runtime/simulator/rpc_client_base.h>\n\n");

  printer.Print("namespace ceq::rt::rpc {\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();

    printer.Print("\n");
    printer.Print(vars, "class $service_name$Client final : private ClientBase {\n");
    printer.Print("public:\n");
    printer.Indent();

    printer.Print("using ClientBase::ClientBase;\n\n");

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

  printer.Print("\n}  // namespace ceq::rt::rpc\n");
}

void GenerateClientSource(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(
      generator_context->Open(file_name + ".client.sim.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.client.sim.h\"\n\n", "name", file_name);

  printer.Print("namespace ceq::rt::rpc {\n");

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

  printer.Print("\n}  // namespace ceq::rt::rpc\n");
}

void GenerateServiceHeader(GeneratorContext* generator_context,
                           const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(
      generator_context->Open(file_name + ".service.sim.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  AddDependencyHeaders(printer, file);
  printer.Print("#include <runtime/simulator/rpc_service_base.h>\n\n");

  printer.Print("namespace ceq::rt::rpc {\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Stub";

    printer.Print("\n");
    printer.Print(vars, "class $service_class$ : public Server::Service {\n");
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

    printer.Print(vars,
                  "Result<SerializedData, RpcError> ProcessRequest(const SerializedData& data, "
                  "const HandlerName& handler_name) noexcept override;\n");

    printer.Outdent();
    printer.Print("};\n");
  }

  printer.Print("\n}  // namespace ceq::rt::rpc\n");
}

void GenerateServiceSource(GeneratorContext* generator_context,
                           const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(
      generator_context->Open(file_name + ".service.sim.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.service.sim.h\"\n\n", "name", file_name);

  printer.Print("namespace ceq::rt::rpc {\n");

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
                  "Server::Service{\"$service_name$\"} {}\n\n");

    printer.Print(vars,
                  "Result<SerializedData, RpcError> $service_class$::ProcessRequest(const "
                  "SerializedData& data, const HandlerName& handler_name) noexcept {\n");
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
    printer.Print(vars, "return Err(RpcErrorType::HandlerNotFound);\n");
    printer.Outdent();
    printer.Print("}\n");
  }

  printer.Print("\n}  // namespace ceq::rt::rpc\n");
}

}  // namespace ceq::codegen::sim
