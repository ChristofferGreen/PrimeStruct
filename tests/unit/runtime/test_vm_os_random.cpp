#include "primec/ir/Ir.h"
#include "primec/ir/IrPreparation.h"
#include "primec/pipeline/CompilePipeline.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmHost.h"
#include "primec/support/Options.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace primec;

// `/std/os` `randomHex` on the VM, which has no `getentropy`: the host function `ps_os_random_bytes` is answered by a
// scripted binding that writes known bytes, so the hex formatting, the length limits and the refusal of a failed call
// are checked without a random source. The compiled-C++ side (the shim and real entropy) is
// tests/unit/backend_host/test_cpp_strings.cpp.
TEST_SUITE_BEGIN("primestruct.runtime.vm_os_random");

namespace {

// The bytes the scripted source writes: byte `i` of a request is `Script[i % size]`.
constexpr uint8_t Script[] = {0x00, 0x0f, 0x10, 0xa5, 0xff, 0x7e, 0x80, 0x09};

const char *Source = R"(
import /std/os/*
import /std/text/*

[return<int> effects(heap_alloc)]
main() {
  [String] six{randomHex(6i32)}
  if(!six.equalsText("000f10a5ff7e")) {
    return(1i32)
  }
  [String] eight{randomHex(8i32)}
  if(!eight.equalsText("000f10a5ff7e8009")) {
    return(2i32)
  }
  [String] nine{randomHex(9i32)}
  if(!nine.equalsText("000f10a5ff7e800900")) {
    return(3i32)
  }
  [String] one{randomHex(1i32)}
  if(!one.equalsText("00")) {
    return(4i32)
  }
  if(randomHex(0i32).count() != 0i32 || randomHex(-5i32).count() != 0i32 || randomHex(65i32).count() != 0i32) {
    return(5i32)
  }
  if(randomHex(64i32).count() != 128i32) {
    return(6i32)
  }
  return(0i32)
}
)";

const char *FailingSource = R"(
import /std/os/*
import /std/text/*

[return<int> effects(heap_alloc)]
main() {
  if(randomHex(16i32).count() != 0i32) {
    return(1i32)
  }
  return(0i32)
}
)";

bool compileVm(const std::string &source, IrModule &module, std::string &error) {
  Options options;
  options.emitKind = "vm";
  options.inputPath =
      std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/unit/runtime/os_random_test.prime";
  options.entryPath = "/main";
  options.inMemorySource = source;
  options.importPaths = {std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib"};
  CompilePipelineOutput output;
  CompilePipelineErrorStage stage = CompilePipelineErrorStage::None;
  if (!runCompilePipeline(options, output, stage, error)) {
    return false;
  }
  IrPreparationFailure failure;
  const SemanticProgram *semanticProgram =
      output.hasSemanticProgram ? &output.semanticProgram : nullptr;
  if (!prepareIrModule(output.program,
                       semanticProgram,
                       options,
                       IrValidationTarget::Vm,
                       module,
                       failure,
                       &output.expandedSource)) {
    error = failure.message;
    return false;
  }
  return true;
}

// `status` is what the host function returns; 0 fills the buffer from `Script`.
VmHostFunctions randomHosts(int32_t status) {
  VmHostFunctions hosts;
  VmHostBinding binding;
  binding.parameters = {IrHostValueKind::BytePointer, IrHostValueKind::U64};
  binding.returnKind = IrHostValueKind::I32;
  binding.invoke = [status](const uint64_t *args, uint64_t &result, std::string &) {
    if (status == 0) {
      uint8_t *buffer = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(args[0]));
      for (uint64_t index = 0; index < args[1]; ++index) {
        buffer[index] = Script[index % sizeof(Script)];
      }
    }
    result = static_cast<uint64_t>(static_cast<int64_t>(status));
    return true;
  };
  hosts.bind("ps_os_random_bytes", std::move(binding));
  return hosts;
}

struct RunResult {
  bool ok = false;
  uint64_t code = 0;
  std::string error;
};

RunResult run(const IrModule &module, const VmHostFunctions &hosts) {
  RunResult result;
  Vm vm;
  result.ok =
      vm.execute(module, result.code, result.error, std::vector<std::string_view>{"prog"}, hosts);
  return result;
}

} // namespace

TEST_CASE(
    "randomHex formats the bytes of the host as lower-case hex and refuses a count outside 1..64") {
  IrModule module;
  std::string error;
  REQUIRE_MESSAGE(compileVm(Source, module, error), error);
  const RunResult result = run(module, randomHosts(0));
  CHECK_MESSAGE(result.ok, result.error);
  CHECK_MESSAGE(result.code == 0, "randomHex case " << result.code << " failed");
}

TEST_CASE("randomHex is empty when the host call fails") {
  IrModule module;
  std::string error;
  REQUIRE_MESSAGE(compileVm(FailingSource, module, error), error);
  const RunResult result = run(module, randomHosts(-100 - 5));
  CHECK_MESSAGE(result.ok, result.error);
  CHECK_MESSAGE(result.code == 0, "randomHex case " << result.code << " failed");
}

TEST_CASE("randomHex on the VM without a binding reports the unbound host function") {
  IrModule module;
  std::string error;
  REQUIRE_MESSAGE(compileVm(FailingSource, module, error), error);
  const RunResult result = run(module, VmHostFunctions{});
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("unbound host function: ps_os_random_bytes") != std::string::npos);
}

TEST_SUITE_END();
