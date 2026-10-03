#include "primec/backend/NativeEmitter.h"
#include "primec/testing/TestScratch.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_random_programs.h"
#include "test_ir_runtime_programs.h"
#include "test_ir_vm_run.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.native_codegen");

// The direct native emitter has an optimizing mode (locals in registers, deferred
// operands, fused compare-and-branch). Every program here must print the same
// output in that mode, in the plain template mode, and in the VM.

#if defined(__linux__) && defined(__x86_64__)

namespace {

using optimizer_test::Outcome;

struct NativeRun {
  bool built = false;
  int exitCode = -1;
  std::string out;
  std::string error;
};

NativeRun
compileNativeAndRun(const primec::IrModule &module, const std::string &name, bool optimized) {
  NativeRun run;
  const std::filesystem::path binary =
      primec::testing::testScratchPath("native_codegen/" + name + (optimized ? ".opt" : ".plain"));
  primec::NativeEmitterOptions options;
  options.promoteLocals = optimized;
  options.deferOperands = optimized;
  if (!primec::NativeEmitter().emitExecutable(
          module, binary.string(), run.error, nullptr, options)) {
    return run;
  }
  run.built = true;
  const std::filesystem::path outPath = binary.string() + ".stdout";
  const int status =
      std::system(("'" + binary.string() + "' > '" + outPath.string() + "' 2>/dev/null").c_str());
  run.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  std::ifstream file(outPath, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  run.out = buffer.str();
  return run;
}

void expectNativeMatchesVm(const primec::IrModule &module, const std::string &name) {
  const Outcome vm = optimizer_test::run(module);
  INFO(name);
  REQUIRE_MESSAGE(vm.ok, vm.error);
  for (const bool optimized : {false, true}) {
    CAPTURE(optimized);
    const NativeRun native = compileNativeAndRun(module, name, optimized);
    REQUIRE_MESSAGE(native.built, native.error);
    CHECK(native.out == vm.output);
    CHECK(native.exitCode ==
          static_cast<int>(static_cast<uint8_t>(static_cast<int32_t>(vm.result))));
  }
}

} // namespace

TEST_CASE("optimized native code matches the VM on random programs") {
  constexpr uint64_t Programs = 60;
  std::vector<primec::IrFunction> bodies;
  for (uint64_t seed = 0; seed < Programs; ++seed) {
    bodies.push_back(optimizer_test::makeModule(seed + 52000).functions[0]);
    bodies.back().name = "/program" + std::to_string(seed);
  }
  expectNativeMatchesVm(optimizer_test::driverModule(std::move(bodies)), "random");
}

TEST_CASE("optimized native code matches the VM on every fused form") {
  expectNativeMatchesVm(optimizer_test::fusedFormsProgram(), "fused_forms");
}

TEST_CASE("optimized native code keeps locals across calls, prints and recursion") {
  expectNativeMatchesVm(optimizer_test::callsProgram(), "calls");

  // A loop whose body calls a function that uses registers for its own locals and
  // prints each iteration: the caller's register locals must survive both.
  primec::IrModule module;
  module.entryIndex = 0;
  module.functions.push_back(optimizer_test::functionOf(
      "/main",
      optimizer_test::assemble({"PushI32 0",    "StoreLocal 0", "PushI64 0",    "StoreLocal 1",
                                "LoadLocal 0",  "PushI32 6",    "CmpLtI32",     "JumpIfZero 22",
                                "LoadLocal 1",  "LoadLocal 0",  "Call 1",       "AddI64",
                                "StoreLocal 1", "LoadLocal 1",  "PrintI64 1",   "LoadLocal 0",
                                "PushI32 1",    "AddI32",       "StoreLocal 0", "Jump 4",
                                "LoadLocal 1",  "PrintI64 1",   "PushI32 0",    "ReturnI32"})));
  module.functions.push_back(optimizer_test::functionOf("/f",
                                                        optimizer_test::assemble({"StoreLocal 0",
                                                                                  "LoadLocal 0",
                                                                                  "LoadLocal 0",
                                                                                  "MulI64",
                                                                                  "PushI64 3",
                                                                                  "AddI64",
                                                                                  "ReturnI64"}),
                                                        1));
  expectNativeMatchesVm(module, "loop_with_calls");
}

TEST_CASE("optimized native code handles deep operand stacks and aliased locals") {
  // Nine operands live at once exceed the cache registers and force the oldest
  // ones onto the memory stack, in order.
  std::vector<primec::IrInstruction> deep;
  deep.push_back({primec::IrOpcode::PushI64, 5});
  deep.push_back({primec::IrOpcode::StoreLocal, 0});
  deep.push_back({primec::IrOpcode::PushI64, 7});
  deep.push_back({primec::IrOpcode::StoreLocal, 1});
  for (int i = 0; i < 9; ++i) {
    deep.push_back({i % 3 == 0 ? primec::IrOpcode::PushI32 : primec::IrOpcode::LoadLocal,
                    i % 3 == 0 ? static_cast<uint64_t>(i + 1) : static_cast<uint64_t>(i % 2)});
  }
  for (int i = 0; i < 4; ++i) {
    deep.push_back({primec::IrOpcode::SubI64, 0});
    deep.push_back({primec::IrOpcode::MulI64, 0});
  }
  deep.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  deep.push_back({primec::IrOpcode::PushI32, 0});
  deep.push_back({primec::IrOpcode::ReturnI32, 0});
  primec::IrModule deepModule = optimizer_test::moduleOf(std::move(deep));
  deepModule.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(deepModule, "deep_stack");

  // `x` is read lazily, overwritten, and read again: the first read must see the
  // old value (10), the second the new one (7), so the sum is 17.
  primec::IrModule alias = optimizer_test::moduleOf(optimizer_test::assemble({"PushI64 10",
                                                                              "StoreLocal 0",
                                                                              "LoadLocal 0",
                                                                              "PushI64 7",
                                                                              "StoreLocal 0",
                                                                              "LoadLocal 0",
                                                                              "AddI64",
                                                                              "PrintI64 1",
                                                                              "LoadLocal 0",
                                                                              "LoadLocal 0",
                                                                              "MulI64",
                                                                              "PrintI64 1",
                                                                              "PushI32 0",
                                                                              "ReturnI32"}));
  alias.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(alias, "alias");
}

TEST_CASE("printing does not clobber locals or operands, in the entry function or a callee") {
  // The print scratch area lives at the top of the frame, below which the digits
  // are written; it used to overlap the locals of a called function and the top
  // of the entry function's operand stack.
  primec::IrModule callee;
  callee.entryIndex = 0;
  callee.functions.push_back(optimizer_test::functionOf(
      "/main", optimizer_test::assemble({"CallVoid 1", "PushI32 0", "ReturnI32"})));
  callee.functions.push_back(optimizer_test::functionOf("/p",
                                                        optimizer_test::assemble({"PushI64 5",
                                                                                  "StoreLocal 0",
                                                                                  "PushI64 6",
                                                                                  "StoreLocal 1",
                                                                                  "PushI64 7",
                                                                                  "StoreLocal 2",
                                                                                  "LoadLocal 2",
                                                                                  "PrintI64 1",
                                                                                  "LoadLocal 1",
                                                                                  "PrintI64 1",
                                                                                  "LoadLocal 0",
                                                                                  "PrintI64 1",
                                                                                  "PushI32 0",
                                                                                  "ReturnI32"})));
  expectNativeMatchesVm(callee, "print_in_callee");

  // Eight operands wait on the stack while a print runs in the middle.
  std::vector<primec::IrInstruction> deepPrint;
  for (int i = 1; i <= 8; ++i) {
    deepPrint.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(i * 111)});
  }
  deepPrint.push_back({primec::IrOpcode::PushI64, 1234567890123456789ull});
  deepPrint.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  for (int i = 0; i < 7; ++i) {
    deepPrint.push_back({primec::IrOpcode::AddI64, 0});
  }
  deepPrint.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  deepPrint.push_back({primec::IrOpcode::PushI32, 0});
  deepPrint.push_back({primec::IrOpcode::ReturnI32, 0});
  primec::IrModule deepPrintModule = optimizer_test::moduleOf(std::move(deepPrint));
  deepPrintModule.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(deepPrintModule, "print_with_deep_stack");
}

TEST_CASE("native heap realloc keeps the common prefix when shrinking and growing") {
  // The block is shrunk from 100 slots to 10 and grown to 1000; the old code
  // lost the new size to the mmap syscall's register clobber and copied the old,
  // larger size into the smaller block.
  primec::IrModule module = optimizer_test::moduleOf(optimizer_test::assemble(
      {"PushI64 100",   "HeapAlloc",     "StoreLocal 0", "LoadLocal 0",  "PushI64 11",
       "StoreIndirect", "Pop",           "LoadLocal 0",  "PushI64 144",  "AddI64",
       "PushI64 22",    "StoreIndirect", "Pop",          "LoadLocal 0",  "PushI64 10",
       "HeapRealloc",   "StoreLocal 0",  "LoadLocal 0",  "LoadIndirect", "PrintI64 1",
       "LoadLocal 0",   "PushI64 144",   "AddI64",       "LoadIndirect", "PrintI64 1",
       "LoadLocal 0",   "PushI64 1000",  "HeapRealloc",  "StoreLocal 0", "LoadLocal 0",
       "LoadIndirect",  "PrintI64 1",    "LoadLocal 0",  "PushI64 144",  "AddI64",
       "LoadIndirect",  "PrintI64 1",    "LoadLocal 0",  "HeapFree",     "PushI32 0",
       "ReturnI32"}));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(module, "heap_realloc");
}

TEST_CASE("i32 slots print and return as 32-bit values, as in the VM") {
  // 2147483647 + 1 stays 2147483648 in the 64-bit slot; the VM reads the low 32
  // bits when printing, writing to a file and returning.
  primec::IrModule module =
      optimizer_test::moduleOf(optimizer_test::assemble({"PushI32 2147483647",
                                                         "PushI32 1",
                                                         "AddI32",
                                                         "Dup",
                                                         "PrintI32 1",
                                                         "PushI64 -4294967296",
                                                         "AddI64",
                                                         "PrintI32 1",
                                                         "PushI64 4294967297",
                                                         "ReturnI32"}));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(module, "i32_widths");
}

#else

TEST_CASE("optimized native code is only built on Linux x86_64") {
  CHECK(true);
}

#endif
