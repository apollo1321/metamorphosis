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

void GenerateClientHeader(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  printer.Print("#include <rpc_client_base.h>\n");

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
      auto method = service->method(method_id);
      vars["method_name"] = method->name();
      vars["output_type"] = method->output_type()->name();
      vars["input_type"] = method->input_type()->name();
      printer.Print(vars, "$output_type$ $method_name$(const $input_type$& input);\n");
    }

    printer.Outdent();
    printer.Print("};\n");
  }
}

void GenerateClientSource(GeneratorContext* generator_context,
                          const FileDescriptor* file) noexcept {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".client.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.client.h\"\n", "name", file_name);

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto service = file->service(service_id);

    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Client";

    // Methods
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      printer.Print("\n");
      auto method = service->method(method_id);
      vars["method_name"] = method->name();
      vars["output_type"] = method->output_type()->name();
      vars["input_type"] = method->input_type()->name();
      printer.Print(vars,
                    "$output_type$ $service_class$::$method_name$(const $input_type$& input) {\n");
      printer.Indent();
      printer.Print(vars,
                    "return MethodImpl<$input_type$, $output_type$>(input, "
                    "\"/$service_name$/$method_name$\");\n");
      printer.Outdent();
      printer.Print("}\n");
    }
  }
}

void GenerateHandlerHeader(GeneratorContext* generator_context, const FileDescriptor* file) {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".handler.h"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#pragma once\n\n");
  printer.Print("#include \"$name$.pb.h\"\n\n", "name", file_name);
  printer.Print("#include <rpc_handler_base.h>\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto put_method_info = [&vars](const auto& method) {
      vars["method_name"] = method->name();
      vars["output_type"] = method->output_type()->name();
      vars["input_type"] = method->input_type()->name();
    };

    auto service = file->service(service_id);
    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Handler";

    printer.Print("\n");
    printer.Print(vars, "class $service_class$ : private RpcHandlerBase {\n");
    printer.Print("public:\n");
    printer.Indent();

    // Constructor
    printer.Print(vars, "$service_class$();\n");
    // Destructor
    printer.Print(vars, "~$service_class$();\n\n");

    // Run
    printer.Print(vars, "using RpcHandlerBase::Run;\n");
    // ShutDown
    printer.Print(vars, "using RpcHandlerBase::ShutDown;\n\n");

    // Placeholders for handlers
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      put_method_info(service->method(method_id));

      printer.Print(vars,
                    "virtual $output_type$ $method_name$(const $input_type$& request) = 0;\n");
    }
    printer.Print("\n");

    printer.Outdent();
    printer.Print("private:\n");
    printer.Indent();

    // Put methods rpc calls in queue
    printer.Print(vars, "void PutAllMethodsCallsInQueue() override;\n\n");

    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      put_method_info(service->method(method_id));

      printer.Print(vars, "void Put$method_name$InQueue();\n");
    }

    // Make rpc calls struct for each method
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      put_method_info(service->method(method_id));

      printer.Print("\n");
      printer.Print(
          vars, "struct RpcCall$method_name$ : public RpcCall<$input_type$, $output_type$> {\n");
      printer.Indent();
      printer.Print(
          vars,
          "explicit RpcCall$method_name$($service_name$Handler* handler) : handler{handler} {}\n");
      printer.Print(vars, "void operator()() noexcept override;\n");
      printer.Print(vars, "void PutNewCallInQueue() noexcept override;\n");
      printer.Print(vars, "$service_class$* handler;\n");
      printer.Outdent();
      printer.Print("};\n");
    }

    printer.Outdent();
    printer.Print("};\n");
  }
}

void GenerateHandlerSource(GeneratorContext* generator_context, const FileDescriptor* file) {
  const std::string file_name = GetProtoFileName(file);

  std::unique_ptr<ZeroCopyOutputStream> stream(generator_context->Open(file_name + ".handler.cc"));
  Printer printer(stream.get(), '$');

  // Includes
  printer.Print("#include \"$name$.handler.h\"\n\n", "name", file_name);

  // Using
  printer.Print("using namespace grpc::internal;  // NOLINT\n");
  printer.Print("using BaseType = grpc::protobuf::MessageLite;\n");

  // Services
  for (int service_id = 0; service_id < file->service_count(); ++service_id) {
    std::map<std::string, std::string> vars;

    auto put_method_info = [&vars](const auto& method) {
      vars["method_name"] = method->name();
      vars["output_type"] = method->output_type()->name();
      vars["input_type"] = method->input_type()->name();
    };

    auto service = file->service(service_id);
    vars["service_name"] = service->name();
    vars["service_class"] = vars["service_name"] + "Handler";

    printer.Print("\n");

    // Constructor
    printer.Print(vars, "$service_class$::$service_class$() {\n");
    printer.Indent();
    printer.Print("auto stub = [](auto, auto, auto, auto) { return SyncMethodStub(); };\n");
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      put_method_info(service->method(method_id));
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

    // Destructor
    printer.Print(vars, "$service_class$::~$service_class$() {\n");
    printer.Indent();
    printer.Print("if (IsRunning()) { ShutDown(); }\n");
    printer.Outdent();
    printer.Print("}\n\n");

    // Put methods rpc calls in queue
    printer.Print(vars, "void $service_class$::PutAllMethodsCallsInQueue() {\n");
    printer.Indent();
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      put_method_info(service->method(method_id));
      printer.Print(vars, "Put$method_name$InQueue();\n");
    }
    printer.Outdent();
    printer.Print("}\n\n");

    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      put_method_info(service->method(method_id));
      vars["id"] = std::to_string(method_id);

      printer.Print(vars, "void $service_class$::Put$method_name$InQueue() {\n");
      printer.Indent();
      printer.Print(vars, "PutRpcCallInQueue($id$, new RpcCall$method_name${this});\n");
      printer.Outdent();
      printer.Print("}\n");
    }

    // struct method implementation
    for (int method_id = 0; method_id < service->method_count(); ++method_id) {
      put_method_info(service->method(method_id));

      printer.Print("\n");
      printer.Print(vars, "void $service_class$::RpcCall$method_name$::operator()() noexcept {\n");
      printer.Indent();
      printer.Print(vars,
                    "RunImpl([this](auto request) { return handler->$method_name$(request); });\n");
      printer.Outdent();
      printer.Print("}\n");

      printer.Print("\n");
      printer.Print(vars,
                    "void $service_class$::RpcCall$method_name$::PutNewCallInQueue() noexcept {\n");
      printer.Indent();
      printer.Print(vars, "handler->Put$method_name$InQueue();\n");
      printer.Outdent();
      printer.Print("}\n");
    }
  }
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

  GenerateHandlerHeader(generator_context, file);
  GenerateHandlerSource(generator_context, file);

  return true;
}
