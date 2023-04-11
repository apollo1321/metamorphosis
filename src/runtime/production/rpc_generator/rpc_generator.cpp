#include "rpc_generator.h"

#include <memory>
#include <stdexcept>

#include <runtime/util/codegen.h>

using google::protobuf::FileDescriptor;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

namespace ceq::rt::rpc {

void GenerateClientHeader(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  AddDependencyHeaders(printer, file);
  printer.Print("#include <runtime/production/rpc_client_base.h>\n\n");

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
                    "Result<$output_type$, Error> $method_name$(const $input_type$& input, "
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

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.client.h\"\n\n", "name", file_name);

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
                    "Result<$output_type$, Error> $service_class$::$method_name$(const "
                    "$input_type$& input, StopToken stop_token) noexcept {\n");
      printer.Indent();
      printer.Print(vars,
                    "return MakeRequest<$input_type$, $output_type$>(input, "
                    "\"/$service_name$/$method_name$\", std::move(stop_token));\n");
      printer.Outdent();
      printer.Print("}\n");
    }
  }

  printer.Print("\n}  // namespace ceq::rt::rpc\n");
}

void GenerateServiceHeader(GeneratorContext* generator_context, const FileDescriptor* file) {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".service.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  AddDependencyHeaders(printer, file);
  printer.Print("#include <runtime/production/rpc_service_base.h>\n");
  printer.Print("#include <util/result.h>\n\n");

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
    printer.Print(vars, "$service_class$();\n\n");

    // Placeholders for services
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));

      printer.Print(vars,
                    "virtual Result<$output_type$, Error> $method_name$(const $input_type$& "
                    "request) noexcept = 0;\n");
    }
    printer.Print("\n");

    printer.Outdent();
    printer.Print("private:\n");
    printer.Indent();

    // Put methods rpc calls in queue
    printer.Print(
        vars, "void PutAllMethodsCallsInQueue(grpc::ServerCompletionQueue& queue) override;\n\n");

    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));

      printer.Print(vars, "void Put$method_name$InQueue(grpc::ServerCompletionQueue& queue);\n");
    }

    // Make rpc calls struct for each method
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));

      printer.Print("\n");
      printer.Print(
          vars, "struct RpcCall$method_name$ : public RpcCall<$input_type$, $output_type$> {\n");
      printer.Indent();
      printer.Print(
          vars, "explicit RpcCall$method_name$($service_class$* handler) : handler{handler} {}\n");
      printer.Print(vars, "void operator()() noexcept override;\n");
      printer.Print(
          vars, "void PutNewCallInQueue(grpc::ServerCompletionQueue& queue) noexcept override;\n");
      printer.Print(vars, "$service_class$* handler;\n");
      printer.Outdent();
      printer.Print("};\n");
    }

    printer.Outdent();
    printer.Print("};\n");
  }

  printer.Print("\n}  // namespace ceq::rt::rpc\n");
}

void GenerateServiceSource(GeneratorContext* generator_context, const FileDescriptor* file) {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".service.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.service.h\"\n\n", "name", file_name);

  printer.Print("namespace ceq::rt::rpc {\n\n");

  // Using
  printer.Print("using namespace grpc::internal;  // NOLINT\n");
  printer.Print("using BaseType = grpc::protobuf::MessageLite;\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);
    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Stub";

    printer.Print("\n");

    // Constructor
    printer.Print(vars, "$service_class$::$service_class$() {\n");
    printer.Indent();
    printer.Print("auto stub = [](auto, auto, auto, auto) { return SyncMethodStub(); };\n");
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));
      vars["id"] = std::to_string(method_id);

      printer.Print("\n");
      printer.Print(vars,
                    "auto handler$id$ = new RpcMethodHandler<$service_class$, $input_type$, "
                    "$output_type$, BaseType, BaseType>(stub, this);\n");
      printer.Print(vars,
                    "auto method$id$ = new RpcServiceMethod(\"/$service_name$/$method_name$\", "
                    "RpcMethod::NORMAL_RPC, handler$id$);\n");
      printer.Print(vars, "grpc::Service::AddMethod(method$id$);\n");
      printer.Print(vars, "grpc::Service::MarkMethodAsync($id$);\n");
    }
    printer.Outdent();
    printer.Print("}\n\n");

    // Put methods rpc calls in queue
    printer.Print(
        vars,
        "void $service_class$::PutAllMethodsCallsInQueue(grpc::ServerCompletionQueue& queue) {\n");
    printer.Indent();
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));
      printer.Print(vars, "Put$method_name$InQueue(queue);\n");
    }
    printer.Outdent();
    printer.Print("}\n\n");

    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));
      vars["id"] = std::to_string(method_id);

      printer.Print(
          vars,
          "void $service_class$::Put$method_name$InQueue(grpc::ServerCompletionQueue& queue) {\n");
      printer.Indent();
      printer.Print(vars, "PutRpcCallInQueue(queue, $id$, new RpcCall$method_name${this});\n");
      printer.Outdent();
      printer.Print("}\n");
    }

    // struct method implementation
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      AddMethodInfo(vars, service->method(method_id));

      printer.Print("\n");
      printer.Print(vars, "void $service_class$::RpcCall$method_name$::operator()() noexcept {\n");
      printer.Indent();
      printer.Print(
          vars,
          "RunImpl([this](const auto& request) { return handler->$method_name$(request); });\n");
      printer.Outdent();
      printer.Print("}\n");

      printer.Print("\n");
      printer.Print(vars,
                    "void "
                    "$service_class$::RpcCall$method_name$::PutNewCallInQueue(grpc::"
                    "ServerCompletionQueue& queue) noexcept {\n");
      printer.Indent();
      printer.Print(vars, "handler->Put$method_name$InQueue(queue);\n");
      printer.Outdent();
      printer.Print("}\n");
    }
  }

  printer.Print("\n}  // namespace ceq::rt::rpc\n");
}

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

}  // namespace ceq::rt::rpc
