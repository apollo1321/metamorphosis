#include "util.h"

namespace mtf::codegen {

bool HasStreaming(const google::protobuf::FileDescriptor* file) noexcept {
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

std::string GetProtoFileName(const google::protobuf::FileDescriptor* file) noexcept {
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

void AddMethodInfo(std::map<std::string, std::string>& vars,
                   const google::protobuf::MethodDescriptor* method) {
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

void AddDependencyHeaders(google::protobuf::io::Printer& printer,
                          const google::protobuf::FileDescriptor* file) noexcept {
  for (int dependency_id = 0; dependency_id < file->dependency_count(); ++dependency_id) {
    auto dependency = file->dependency(dependency_id);
    auto proto_path = dependency->name();
    proto_path = proto_path.substr(0, proto_path.size() - sizeof(".proto") + 1);
    printer.Print("#include <$path$.pb.h>\n", "path", proto_path);
  }
  printer.Print("\n");
}

}  // namespace mtf::codegen
