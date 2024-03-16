#include "rpc_generator.h"

#include <google/protobuf/compiler/plugin.h>

int main(int argc, char** argv) {
  mtf::codegen::RpcGenerator generator;

  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
