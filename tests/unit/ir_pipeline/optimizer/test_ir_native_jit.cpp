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

TEST_CASE("native JIT addresses frame locals like the interpreter") {
  // Address values are the VM's byte offsets (printed, compared and stepped through), locals
  // reached only through an address start at zero, and an address handed to another function
  // names a slot of that function's own frame, as in the VM.
  primec::IrModule module = moduleFrom(
      {"PushI64 11",   "StoreLocal 0", "PushI64 22",    "StoreLocal 1", "AddressOfLocal 0",
       "StoreLocal 4", "LoadLocal 4",  "PrintI64 1",    "LoadLocal 4",  "PushI64 16",
       "AddI64",       "LoadIndirect", "PrintI64 1",    "LoadLocal 4",  "PushI64 48",
       "AddI64",       "LoadIndirect", "PrintI64 1",    "LoadLocal 4",  "PushI64 32",
       "AddI64",       "PushI64 77",   "StoreIndirect", "PrintI64 1",   "LoadLocal 2",
       "PrintI64 1",   "PushI64 16",   "Call 1",        "PrintI64 1",   "PushI32 0",
       "ReturnI32"});
  module.functions.push_back(
      optimizer_test::functionOf("/callee",
                                 {optimizer_test::assembleOne("StoreLocal 0"),
                                  optimizer_test::assembleOne("PushI64 5"),
                                  optimizer_test::assembleOne("StoreLocal 1"),
                                  optimizer_test::assembleOne("LoadLocal 0"),
                                  optimizer_test::assembleOne("LoadIndirect"),
                                  optimizer_test::assembleOne("ReturnI64")},
                                 1));
  expectSame(module, "frame addresses");
  CHECK(runJit(module).outcome.output == "0\n22\n0\n77\n77\n5\n");

  struct Case {
    const char *name;
    const char *address;
    const char *message;
  };
  const std::vector<Case> cases = {
      {"unaligned", "24", "unaligned indirect address in IR: 24"},
      {"past_the_locals", "64", "invalid indirect address in IR: 64"},
      {"tagged", "9223372036854775824", "invalid indirect address in IR: 9223372036854775824"},
  };
  for (const Case &testCase : cases) {
    for (const bool store : {false, true}) {
      INFO(testCase.name);
      INFO(store);
      std::vector<std::string> lines = {
          "AddressOfLocal 3", "Pop", std::string("PushI64 ") + testCase.address};
      if (store) {
        lines.insert(lines.end(), {"PushI64 1", "StoreIndirect"});
      } else {
        lines.push_back("LoadIndirect");
      }
      lines.push_back("ReturnI64");
      const primec::IrModule faulting = moduleFrom(lines);
      const JitRun jit = runJit(faulting);
      REQUIRE(jit.executed);
      CHECK(jit.outcome == runVm(faulting));
      CHECK(jit.outcome.error == testCase.message);
    }
  }
}

TEST_CASE("native JIT runs heap programs like the interpreter") {
  // Heap addresses are the VM's (tagged, 16 bytes per slot), loads and stores reach live slots
  // only, reallocation copies and frees, and every misuse faults with the VM's message.
  expectSame(optimizer_test::heapProgram(), "heap");

  const std::vector<std::string> prefix = {
      "PushI64 2", "HeapAlloc", "StoreLocal 0", "LoadLocal 0", "PushI64 5", "StoreIndirect", "Pop"};
  struct Case {
    const char *name;
    std::vector<std::string> code;
  };
  const std::vector<Case> cases = {
      {"load_after_free", {"LoadLocal 0", "HeapFree", "LoadLocal 0", "LoadIndirect", "ReturnI64"}},
      {"store_after_free",
       {"LoadLocal 0", "HeapFree", "LoadLocal 0", "PushI64 1", "StoreIndirect", "ReturnI64"}},
      {"past_the_heap", {"LoadLocal 0", "PushI64 32", "AddI64", "LoadIndirect", "ReturnI64"}},
      {"unaligned_heap", {"LoadLocal 0", "PushI64 8", "AddI64", "LoadIndirect", "ReturnI64"}},
      {"double_free",
       {"LoadLocal 0", "HeapFree", "LoadLocal 0", "HeapFree", "PushI64 0", "ReturnI64"}},
      {"free_inside",
       {"LoadLocal 0", "PushI64 16", "AddI64", "HeapFree", "PushI64 0", "ReturnI64"}},
      {"free_frame_address", {"AddressOfLocal 0", "HeapFree", "PushI64 0", "ReturnI64"}},
      {"realloc_after_free",
       {"LoadLocal 0", "HeapFree", "LoadLocal 0", "PushI64 4", "HeapRealloc", "ReturnI64"}},
      {"old_block_after_realloc",
       {"LoadLocal 0",
        "PushI64 4",
        "HeapRealloc",
        "Pop",
        "LoadLocal 0",
        "LoadIndirect",
        "ReturnI64"}},
      {"realloc_shrinks_and_copies",
       {"LoadLocal 0", "PushI64 1", "HeapRealloc", "LoadIndirect", "ReturnI64"}},
      {"realloc_to_zero_frees",
       {"LoadLocal 0",
        "PushI64 0",
        "HeapRealloc",
        "PrintU64 1",
        "LoadLocal 0",
        "LoadIndirect",
        "ReturnI64"}},
      {"zero_slots_is_null",
       {"PushI64 0",
        "HeapAlloc",
        "PrintU64 1",
        "PushI64 0",
        "HeapFree",
        "LoadLocal 0",
        "LoadIndirect",
        "ReturnI64"}},
  };
  for (const Case &testCase : cases) {
    std::vector<std::string> lines = prefix;
    lines.insert(lines.end(), testCase.code.begin(), testCase.code.end());
    expectSame(moduleFrom(lines), testCase.name);
  }
}

TEST_CASE("native JIT runs strings, argv and files like the interpreter") {
  const std::filesystem::path file = primec::testing::testScratchPath("native_jit/io.txt");
  std::filesystem::create_directories(file.parent_path());
  primec::IrModule module = optimizer_test::ioProgram(file.string());
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  std::string reason;
  REQUIRE_MESSAGE(primec::nativeJitAccepts(module, reason), reason);
  const std::vector<std::string_view> args = {"program", "first", "se cond"};
  const JitRun jit = runJit(module, args);
  REQUIRE(jit.executed);
  CHECK(jit.outcome == runVm(module, args));
  CHECK(jit.outcome.ok);

  // Dynamic string operations on indices that name no string.
  for (const char *op : {"PrintStringDynamic 1", "LoadStringByteDynamic"}) {
    INFO(op);
    std::vector<std::string> lines = {"PushI64 5"};
    if (std::string_view(op) == "LoadStringByteDynamic") {
      lines.push_back("PushI64 0");
    }
    lines.insert(lines.end(), {op, "PushI32 0", "ReturnI32"});
    primec::IrModule faulting = moduleFrom(lines);
    faulting.stringTable = {"abc"};
    expectSame(faulting, op);
  }
}

TEST_CASE("native JIT computes f32 values and float conversions like the interpreter") {
  // f32 arithmetic and the conversions without machine code run in the runtime; the results
  // (zero-extended f32 bits, saturated or NaN-mapped integers) travel through the registers.
  const std::vector<std::string> constants = {"0x3fc00000",  // 1.5
                                              "0xc0200000",  // -2.5
                                              "0x7fc00000",  // NaN
                                              "0x7f800000",  // inf
                                              "0x4f800000"}; // 2^32
  std::vector<std::string> lines;
  for (const std::string &a : constants) {
    for (const std::string &b : constants) {
      for (const char *op : {"AddF32", "SubF32", "MulF32", "DivF32", "CmpLtF32", "CmpEqF32"}) {
        lines.insert(lines.end(), {"PushF32 " + a, "PushF32 " + b, op, "PrintU64 1"});
      }
    }
    for (const char *op :
         {"NegF32", "ConvertF32ToI32", "ConvertF32ToI64", "ConvertF32ToU64", "ConvertF32ToF64"}) {
      lines.insert(lines.end(), {"PushF32 " + a, op, "PrintU64 1"});
    }
  }
  for (const char *value : {"-1", "5000000000", "-9223372036854775808", "7"}) {
    for (const char *op : {"ConvertI64ToF32", "ConvertU64ToF32", "ConvertI32ToF32"}) {
      lines.insert(lines.end(), {std::string("PushI64 ") + value, op, "PrintU64 1"});
    }
    lines.insert(
        lines.end(),
        {std::string("PushI64 ") + value, "ConvertI64ToF64", "ConvertF64ToI32", "PrintU64 1"});
    lines.insert(
        lines.end(),
        {std::string("PushI64 ") + value, "ConvertI64ToF64", "ConvertF64ToU64", "PrintU64 1"});
    lines.insert(
        lines.end(),
        {std::string("PushI64 ") + value, "ConvertI64ToF64", "ConvertF64ToF32", "PrintU64 1"});
  }
  lines.insert(lines.end(),
               {"PushF32 0x3fc00000", "ConvertF32ToF64", "ConvertF64ToI64", "ReturnI32"});
  const primec::IrModule module = moduleFrom(lines);
  std::string reason;
  REQUIRE_MESSAGE(primec::nativeJitAccepts(module, reason), reason);
  expectSame(module, "f32");
}

TEST_CASE("modules importing host functions stay on the interpreter") {
  primec::IrModule module = moduleFrom({"PushI32 0", "ReturnI32"});
  module.hostImports.push_back({});
  module.hostImports.back().name = "answer";
  std::string reason;
  CHECK_FALSE(primec::nativeJitAccepts(module, reason));
  CHECK(reason == "host imports");
  CHECK_FALSE(runJit(module).executed);
}

#endif

TEST_SUITE_END();
