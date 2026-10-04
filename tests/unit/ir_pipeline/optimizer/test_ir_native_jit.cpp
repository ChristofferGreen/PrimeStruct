#include "primec/backend/NativeJit.h"
#include "primec/ir/IrOptimizer.h"
#include "primec/testing/TestScratch.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_random_programs.h"
#include "test_ir_runtime_programs.h"
#include "test_ir_vm_run.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(__linux__) && defined(__x86_64__)
#include <fcntl.h>
#include <unistd.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.native_jit");

// primevm runs a module as native code (primec/backend/NativeJit.h) when that is observably the
// same as interpreting it. Every test runs a module both ways and compares result, printed
// output and fault message.

#if defined(__linux__) && defined(__x86_64__)

namespace {

struct JitRun {
  bool executed = false;
  optimizer_test::Outcome outcome;
};

// The JIT writes to file descriptor 1 itself; point it at a file for the run.
JitRun runJit(const primec::IrModule &module, const std::vector<std::string_view> &args = {}) {
  const std::filesystem::path path = primec::testing::testScratchPath("native_jit/stdout.txt");
  std::filesystem::create_directories(path.parent_path());
  std::fflush(nullptr);
  const int file = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  REQUIRE(file >= 0);
  const int savedStdout = dup(1);
  dup2(file, 1);
  const primec::NativeJitResult result = primec::runNativeJit(module, args);
  dup2(savedStdout, 1);
  close(savedStdout);
  close(file);
  JitRun run;
  run.executed = result.executed;
  run.outcome.ok = result.ok;
  run.outcome.result = result.ok ? result.result : 0;
  run.outcome.error = result.error;
  std::ifstream input(path, std::ios::binary);
  std::stringstream text;
  text << input.rdbuf();
  run.outcome.output = text.str();
  return run;
}

optimizer_test::Outcome runVm(const primec::IrModule &module,
                              const std::vector<std::string_view> &args = {}) {
  optimizer_test::Outcome outcome = optimizer_test::run(module, args);
  if (!outcome.ok) {
    outcome.result = 0;
  }
  return outcome;
}

primec::IrModule moduleFrom(const std::vector<std::string> &lines) {
  std::vector<primec::IrInstruction> code;
  for (const std::string &line : lines) {
    code.push_back(optimizer_test::assembleOne(line));
  }
  primec::IrModule module = optimizer_test::moduleOf(std::move(code));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  return module;
}

void expectSame(const primec::IrModule &module, const std::string &name) {
  INFO(name);
  const JitRun jit = runJit(module);
  REQUIRE(jit.executed);
  CHECK(jit.outcome == runVm(module));
}

} // namespace

TEST_CASE("native JIT agrees with the interpreter on random programs, optimized or not") {
  for (uint64_t seed = 0; seed < 300; ++seed) {
    const primec::IrModule module = optimizer_test::makeModule(seed + 91000);
    std::string reason;
    REQUIRE_MESSAGE(primec::nativeJitAccepts(module, reason), reason);
    expectSame(module, "seed " + std::to_string(seed));

    primec::IrModule optimized = module;
    primec::OptimizationOptions options;
    options.level = 2;
    primec::IrOptimizationReport report;
    std::string error;
    REQUIRE_MESSAGE(
        primec::optimizeIrModule(optimized, options, primec::IrValidationTarget::Vm, report, error),
        error);
    expectSame(optimized, "optimized seed " + std::to_string(seed));
  }
}

TEST_CASE("native JIT agrees with the interpreter on calls and recursion") {
  expectSame(optimizer_test::callsProgram(), "calls");
}

TEST_CASE("native JIT reports the interpreter's faults") {
  // Shared fault programs: the ones the JIT takes must fault exactly like the VM.
  size_t taken = 0;
  for (const optimizer_test::FaultProgram &program : optimizer_test::faultPrograms()) {
    std::string reason;
    if (!primec::nativeJitAccepts(program.module, reason)) {
      continue;
    }
    ++taken;
    INFO(program.name);
    const JitRun jit = runJit(program.module);
    REQUIRE(jit.executed);
    const optimizer_test::Outcome vm = runVm(program.module);
    CHECK_FALSE(vm.ok);
    CHECK(jit.outcome == vm);
  }
  CHECK(taken >= 5);

  struct Case {
    const char *name;
    std::vector<std::string> code;
    const char *message;
  };
  const std::vector<Case> cases = {
      {"signed_division_by_zero",
       {"PushI64 7", "PushI64 0", "DivI32", "ReturnI32"},
       "division by zero in IR"},
      {"string_byte_negative",
       {"PushI64 -1", "LoadStringByte 0", "ReturnI32"},
       "string index out of bounds in IR"},
      {"string_byte_at_end",
       {"PushI64 3", "LoadStringByte 0", "ReturnI32"},
       "string index out of bounds in IR"},
      {"dynamic_string_length",
       {"PushI64 9223372036854775808", "LoadStringLength", "ReturnI64"},
       "invalid dynamic string index in IR"},
      {"missing_return_in_entry", {"PushI32 1", "Pop"}, "missing return in IR"},
  };
  for (const Case &testCase : cases) {
    INFO(testCase.name);
    primec::IrModule module = moduleFrom(testCase.code);
    module.stringTable = {"abc"};
    const JitRun jit = runJit(module);
    REQUIRE(jit.executed);
    CHECK(jit.outcome == runVm(module));
    CHECK(jit.outcome.error == testCase.message);
  }

  // Falling off the end of a called function, and recursion past the VM's 4096 frames.
  primec::IrModule fallsOff = moduleFrom({"CallVoid 1", "PushI32 0", "ReturnI32"});
  fallsOff.functions.push_back(optimizer_test::functionOf("/falls_off", {}));
  fallsOff.functions[1].instructions = {optimizer_test::assembleOne("PushI32 1"),
                                        optimizer_test::assembleOne("Pop")};
  JitRun jit = runJit(fallsOff);
  REQUIRE(jit.executed);
  CHECK(jit.outcome == runVm(fallsOff));
  CHECK(jit.outcome.error == "missing return in IR function /falls_off");
}

TEST_CASE("native JIT wraps INT64_MIN divided by -1 like the interpreter") {
  for (const char *op : {"DivI64", "DivI32"}) {
    INFO(op);
    const primec::IrModule module = moduleFrom({"PushI64 -9223372036854775808",
                                                "PushI64 -1",
                                                op,
                                                "PrintI64 1",
                                                "PushI64 -7",
                                                "PushI64 -1",
                                                op,
                                                "PrintI64 1",
                                                "PushI32 0",
                                                "ReturnI32"});
    expectSame(module, op);
  }
}

TEST_CASE("native JIT passes argc like the interpreter") {
  const primec::IrModule module = moduleFrom({"PushArgc", "PrintI32 1", "PushArgc", "ReturnI32"});
  const std::vector<std::string_view> args = {"program", "a", "bb"};
  const JitRun jit = runJit(module, args);
  REQUIRE(jit.executed);
  CHECK(jit.outcome == runVm(module, args));
  CHECK(jit.outcome.output == "3\n");
}

TEST_CASE("modules outside the native JIT subset stay on the interpreter") {
  std::string reason;
  CHECK_FALSE(primec::nativeJitAccepts(optimizer_test::heapProgram(), reason));
  const primec::IrModule floats =
      moduleFrom({"PushF32 0x3f800000", "ConvertF32ToF64", "ConvertF64ToI64", "ReturnI32"});
  CHECK_FALSE(primec::nativeJitAccepts(floats, reason));
  const JitRun jit = runJit(floats);
  CHECK_FALSE(jit.executed);
}

#endif

TEST_SUITE_END();
