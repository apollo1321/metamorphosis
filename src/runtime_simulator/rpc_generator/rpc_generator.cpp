#include "rpc_generator.h"

#include <memory>
#include <stdexcept>

#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

using google::protobuf::FileDescriptor;
using google::protobuf::MethodDescriptor;
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

std::string PackageToNamespace(const std::string& package) noexcept {
  std::string result;
  for (auto ch : package) {
    if (ch == '.') {
      result += "::";
    } else {
      result += ch;
    }
  }
  return result;
}

void AddMethodInfo(std::map<std::string, std::string>& vars, const MethodDescriptor* method) {
  vars["method_name"] = method->name();
  const auto output_ns = PackageToNamespace(method->output_type()->file()->package());
  if (!output_ns.empty()) {
    vars["output_type"] = output_ns + "::" + method->output_type()->name();
  } else {
    vars["output_type"] = method->output_type()->name();
  }
  const auto input_ns = PackageToNamespace(method->input_type()->file()->package());
  if (!input_ns.empty()) {
    vars["input_type"] = input_ns + "::" + method->input_type()->name();
  } else {
    vars["input_type"] = method->input_type()->name();
  }
}

void AddDependencyHeaders(Printer& printer, const FileDescriptor* file) noexcept {
  for (int dependency_id = 0; dependency_id < file->dependency_count(); ++dependency_id) {
    auto dependency = file->dependency(dependency_id);
    auto proto_path = dependency->name();
    proto_path = proto_path.substr(0, proto_path.size() - sizeof(".proto") + 1);
    printer.Print("#include <$path$.pb.h>\n", "path", proto_path);
  }
  printer.Print("\n");
}

void GenerateClientHeader(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(
      generator_context->Open(file_name + ".sim.client.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  AddDependencyHeaders(printer, file);
  printer.Print("#include <runtime_simulator/rpc_client_base.h>\n\n");

  printer.Print("namespace runtime_simulation {\n");

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
      printer.Print(
          vars,
          "Result<$output_type$, RpcError> $method_name$(const $input_type$& input) noexcept;\n");
    }

    printer.Outdent();
    printer.Print("};\n");
  }

  printer.Print("\n}  // namespace runtime_simulation\n");
}

void GenerateClientSource(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(
      generator_context->Open(file_name + ".sim.client.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.sim.client.h\"\n\n", "name", file_name);

  printer.Print("namespace runtime_simulation {\n");

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
                    "$input_type$& input) noexcept {\n");
      printer.Indent();
      printer.Print(vars,
                    "return MakeRequest<$input_type$, $output_type$>(input, "
                    "\"$service_name$\", \"$method_name$\");\n");
      printer.Outdent();
      printer.Print("}\n");
    }
  }

  printer.Print("\n}  // namespace runtime_simulation\n");
}

void GenerateServiceHeader(GeneratorContext* generator_context, const FileDescriptor* file) {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(
      generator_context->Open(file_name + ".sim.service.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  AddDependencyHeaders(printer, file);
  printer.Print("#include <runtime_simulator/rpc_service_base.h>\n\n");

  printer.Print("namespace runtime_simulation {\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Stub";

    printer.Print("\n");
    printer.Print(vars, "class $service_class$ : public RpcServiceBase {\n");
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

  printer.Print("\n}  // namespace runtime_simulation\n");
}

void GenerateServiceSource(GeneratorContext* generator_context, const FileDescriptor* file) {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(
      generator_context->Open(file_name + ".sim.service.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.sim.service.h\"\n\n", "name", file_name);

  printer.Print("namespace runtime_simulation {\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Stub";

    printer.Print("\n");

    // Constructor
    printer.Print(
        vars,
        "$service_class$::$service_class$() noexcept : RpcServiceBase{\"$service_name$\"} {}\n\n");

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
    printer.Print(vars, "return RpcResult::Err(RpcError::ErrorType::HandlerNotFound);\n");
    printer.Outdent();
    printer.Print("}\n");
  }

  printer.Print("\n}  // namespace runtime_simulation\n");
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
