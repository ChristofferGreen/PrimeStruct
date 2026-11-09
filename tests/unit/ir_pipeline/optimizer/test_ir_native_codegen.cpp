#include "primec/backend/NativeEmitter.h"
#include "primec/testing/NativeEmitterEncodings.h"
#include "primec/testing/TestScratch.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_random_programs.h"
#include "test_ir_runtime_programs.h"
#include "test_ir_vm_run.h"

#include <algorithm>
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
// operands, fused compare-and-branch on x86_64; the register allocator on arm64, TODO-5772).
// Every program here must print the same output in that mode, in the plain template mode, and
// in the VM.

#if (defined(__linux__) && defined(__x86_64__)) ||                                                 \
    (defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)))
#define PRIMESTRUCT_NATIVE_CODEGEN_RUNS 1
#endif
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
#define PRIMESTRUCT_NATIVE_CODEGEN_ARM64 1
#endif

#if defined(PRIMESTRUCT_NATIVE_CODEGEN_RUNS)

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
#if defined(PRIMESTRUCT_NATIVE_CODEGEN_ARM64)
  options.registerAllocation = optimized;
#endif
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

#if !defined(PRIMESTRUCT_NATIVE_CODEGEN_ARM64)
// x86_64 only: the arm64 templates print and return the whole 64-bit slot.
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
#endif

TEST_CASE("SextI32 wraps i32 results in registers, deferred operands and promoted updates") {
  // The loop body updates two locals with `local = local OP constant; sext`, the
  // pattern the native emitter fuses into one instruction on a promoted register.
  primec::IrModule module = optimizer_test::moduleOf(optimizer_test::assemble({"PushI64 2147483000",
                                                                               "StoreLocal 0",
                                                                               "PushI32 700",
                                                                               "StoreLocal 1",
                                                                               "LoadLocal 1",
                                                                               "JumpIfZero 19",
                                                                               "LoadLocal 0",
                                                                               "PushI32 1",
                                                                               "AddI32",
                                                                               "SextI32",
                                                                               "StoreLocal 0",
                                                                               "LoadLocal 1",
                                                                               "PushI32 1",
                                                                               "SubI32",
                                                                               "SextI32",
                                                                               "StoreLocal 1",
                                                                               "LoadLocal 0",
                                                                               "Pop",
                                                                               "Jump 4",
                                                                               "LoadLocal 0",
                                                                               "PrintI32 1",
                                                                               "LoadLocal 0",
                                                                               "LoadLocal 0",
                                                                               "MulI32",
                                                                               "SextI32",
                                                                               "PrintI32 1",
                                                                               "PushI64 4294967297",
                                                                               "SextI32",
                                                                               "PrintI32 1",
                                                                               "LoadLocal 0",
                                                                               "LoadLocal 0",
                                                                               "AddI32",
                                                                               "SextI32",
                                                                               "PrintI64 1",
                                                                               "PushI32 0",
                                                                               "ReturnI32"}));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  const Outcome vm = optimizer_test::run(module);
  REQUIRE_MESSAGE(vm.ok, vm.error);
  CHECK(vm.output == "-2147483596\n2704\n1\n104\n");
  expectNativeMatchesVm(module, "sext_i32");
}

#if defined(PRIMESTRUCT_NATIVE_CODEGEN_ARM64)
// arm64 register allocation (TODO-5772): which functions it takes, the code it writes for a loop,
// spills past the pool, and the two-word branches of a function too large for one-word ones.
namespace {

// The register allocation report of `module` emitted with the allocator on.
primec::NativeEmitterInstrumentation arm64AllocationReport(const primec::IrModule &module,
                                                           const std::string &name) {
  primec::NativeEmitterInstrumentation instrumentation;
  primec::NativeEmitterOptions options;
  options.registerAllocation = true;
  std::string error;
  const std::filesystem::path binary =
      primec::testing::testScratchPath("native_codegen/" + name + ".report");
  std::filesystem::create_directories(binary.parent_path());
  CHECK_MESSAGE(primec::NativeEmitter().emitExecutable(
                    module, binary.string(), error, &instrumentation, options),
                error);
  return instrumentation;
}

struct DisassembledInstruction {
  uint64_t address = 0;
  std::string text; // mnemonic and operands, one space apart
};

// `otool -tv` of the executable, one entry per instruction.
std::vector<DisassembledInstruction> disassemble(const std::filesystem::path &binary) {
  const std::filesystem::path listingPath = binary.string() + ".otool";
  std::system(
      ("otool -tv '" + binary.string() + "' > '" + listingPath.string() + "' 2>/dev/null").c_str());
  std::ifstream listing(listingPath);
  std::vector<DisassembledInstruction> instructions;
  std::string line;
  while (std::getline(listing, line)) {
    const size_t tab = line.find('\t');
    if (tab == std::string::npos || tab == 0) {
      continue;
    }
    DisassembledInstruction instruction;
    instruction.address = std::stoull(line.substr(0, tab), nullptr, 16);
    std::string text = line.substr(tab + 1);
    std::replace(text.begin(), text.end(), '\t', ' ');
    instruction.text = text;
    instructions.push_back(instruction);
  }
  return instructions;
}

} // namespace

TEST_CASE("arm64 register allocation keeps a counting loop's locals in registers") {
  // sum = 0; count = 10; while (count > 0) { sum = sum + count; count = count - 1 }; print sum
  primec::IrModule module = optimizer_test::moduleOf(optimizer_test::assemble(
      {"PushI64 0",   "StoreLocal 0",  "PushI64 10",  "StoreLocal 1", "LoadLocal 1", "PushI64 0",
       "CmpGtI64",    "JumpIfZero 17", "LoadLocal 0", "LoadLocal 1",  "AddI64",      "StoreLocal 0",
       "LoadLocal 1", "PushI64 1",     "SubI64",      "StoreLocal 1", "Jump 4",      "LoadLocal 0",
       "PrintI64 1",  "PushI32 0",     "ReturnI32"}));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(module, "regalloc_loop");

  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(module, "regalloc_loop");
  REQUIRE(report.perFunction.size() == 1);
  CHECK(report.perFunction[0].registerAllocated);
  CHECK(report.perFunction[0].registerAllocationSpillSlots == 0);

  // The loop is the compare fused into its branch, the two updates on their registers and the
  // jump back: no frame slot or value-stack access. count is the hottest class, so it takes the
  // first register of the pool (x21), sum the second.
  const std::vector<DisassembledInstruction> code =
      disassemble(primec::testing::testScratchPath("native_codegen/regalloc_loop.opt"));
  size_t start = 0;
  while (start < code.size() && code[start].text != "mov x22, #0x0") {
    ++start;
  }
  REQUIRE(start + 7 <= code.size());
  const std::vector<std::string> expected = {"mov x22, #0x0",
                                             "mov x21, #0xa",
                                             "cmp x21, #0x0",
                                             "b.le",
                                             "add x22, x22, x21",
                                             "sub x21, x21, #0x1",
                                             "b"};
  for (size_t i = 0; i < expected.size(); ++i) {
    CAPTURE(i);
    const std::string &text = code[start + i].text;
    CHECK(text.substr(0, std::min(text.size(), text.find(" 0x"))) == expected[i]);
  }
  // b.le leaves the loop for the print after it; b goes back to the compare.
  CHECK(code[start + 3].text == "b.le 0x" + [&] {
    std::ostringstream hex;
    hex << std::hex << code[start + 7].address;
    return hex.str();
  }());
  std::ostringstream loop;
  loop << "b 0x" << std::hex << code[start + 2].address;
  CHECK(code[start + 6].text == loop.str());

  // TODO-5771: the print template takes sum straight from its register (its push and pop became
  // one move) and branches on the sign with one word; nothing touches the value stack.
  REQUIRE(start + 11 <= code.size());
  CHECK(code[start + 7].text == "add x0, x22, #0x0");
  CHECK(code[start + 8].text == "cmp x0, xzr");
  CHECK(code[start + 9].text.rfind("b.ge 0x", 0) == 0);
  CHECK(code[start + 10].text.rfind("b ", 0) != 0);
  for (const DisassembledInstruction &instruction : code) {
    CHECK(instruction.text.find("[x28") == std::string::npos);
  }
}

TEST_CASE("arm64 register allocation spills values past its pool to frame slots") {
  // 40 locals live across a loop exceed the 18 registers of the pool.
  constexpr uint64_t Locals = 40;
  std::vector<primec::IrInstruction> body;
  for (uint64_t k = 0; k < Locals; ++k) {
    body.push_back({primec::IrOpcode::PushI64, k * 7 + 1});
    body.push_back({primec::IrOpcode::StoreLocal, k});
  }
  const uint64_t counter = Locals;
  body.push_back({primec::IrOpcode::PushI64, 5});
  body.push_back({primec::IrOpcode::StoreLocal, counter});
  const uint64_t header = body.size();
  body.push_back({primec::IrOpcode::LoadLocal, counter});
  const size_t exitBranch = body.size();
  body.push_back({primec::IrOpcode::JumpIfZero, 0});
  for (uint64_t k = 0; k < Locals; ++k) {
    // local[k] = local[k] * 3 + local[k - 1] - k
    body.push_back({primec::IrOpcode::LoadLocal, k});
    body.push_back({primec::IrOpcode::PushI64, 3});
    body.push_back({primec::IrOpcode::MulI64, 0});
    body.push_back({primec::IrOpcode::LoadLocal, (k + Locals - 1) % Locals});
    body.push_back({primec::IrOpcode::AddI64, 0});
    body.push_back({primec::IrOpcode::PushI64, k});
    body.push_back({primec::IrOpcode::SubI64, 0});
    body.push_back({primec::IrOpcode::StoreLocal, k});
  }
  body.push_back({primec::IrOpcode::LoadLocal, counter});
  body.push_back({primec::IrOpcode::PushI64, 1});
  body.push_back({primec::IrOpcode::SubI64, 0});
  body.push_back({primec::IrOpcode::StoreLocal, counter});
  body.push_back({primec::IrOpcode::Jump, header});
  body[exitBranch].imm = body.size();
  for (uint64_t k = 0; k < Locals; ++k) {
    body.push_back({primec::IrOpcode::LoadLocal, k});
    body.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  }
  body.push_back({primec::IrOpcode::PushI32, 0});
  body.push_back({primec::IrOpcode::ReturnI32, 0});
  primec::IrModule module = optimizer_test::moduleOf(std::move(body));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(module, "regalloc_spills");

  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(module, "regalloc_spills");
  REQUIRE(report.perFunction.size() == 1);
  CHECK(report.perFunction[0].registerAllocated);
  CHECK(report.perFunction[0].registerAllocationSpillSlots > 0);
}

TEST_CASE("arm64 register allocation takes calls, addresses and floats") {
  // main calls the others; main (which calls), /address (which takes a local address), /float
  // (float values) and /leaf are all register-allocated.
  primec::IrModule module;
  module.entryIndex = 0;
  module.functions.push_back(optimizer_test::functionOf("/main",
                                                        optimizer_test::assemble({"PushI64 6",
                                                                                  "Call 1",
                                                                                  "PrintI64 1",
                                                                                  "PushI64 7",
                                                                                  "Call 2",
                                                                                  "PrintI64 1",
                                                                                  "PushI64 8",
                                                                                  "Call 3",
                                                                                  "PrintI64 1",
                                                                                  "PushI32 0",
                                                                                  "ReturnI32"})));
  // /address: local 1 = argument, then written and read back through its address.
  module.functions.push_back(
      optimizer_test::functionOf("/address",
                                 optimizer_test::assemble({"StoreLocal 1",
                                                           "AddressOfLocal 1",
                                                           "LoadLocal 1",
                                                           "PushI64 100",
                                                           "AddI64",
                                                           "StoreIndirect",
                                                           "Pop",
                                                           "LoadLocal 1",
                                                           "ReturnI64"}),
                                 1));
  // /float: the argument converted, doubled as a float and converted back.
  module.functions.push_back(optimizer_test::functionOf(
      "/float",
      optimizer_test::assemble(
          {"ConvertI64ToF64", "Dup", "AddF64", "ConvertF64ToI64", "ReturnI64"}),
      1));
  module.functions.push_back(optimizer_test::functionOf(
      "/leaf",
      optimizer_test::assemble(
          {"StoreLocal 0", "LoadLocal 0", "LoadLocal 0", "MulI64", "ReturnI64"}),
      1));
  expectNativeMatchesVm(module, "regalloc_fallbacks");

  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(module, "regalloc_fallbacks");
  REQUIRE(report.perFunction.size() == 4);
  CHECK(report.perFunction[0].registerAllocated);
  CHECK(report.perFunction[1].registerAllocated);
  CHECK(report.perFunction[2].registerAllocated);
  CHECK(report.perFunction[3].registerAllocated);
}

TEST_CASE("arm64 register allocation keeps values live across calls and passes arguments") {
  // /main holds 16 values across two calls of /wide (16 parameters: eight in x0-x7, eight on the
  // value stack), once in reverse order, and prints them all after. /wide's own values take
  // x3-x8 too, so the registers main keeps are overwritten. /floats, with float values, also
  // calls /weights; it is register-allocated like /main, so /weights takes its arguments in
  // registers from both.
  constexpr uint64_t Values = 16;
  std::vector<primec::IrInstruction> main;
  for (uint64_t k = 0; k < Values; ++k) {
    main.push_back({primec::IrOpcode::PushI64, k * 7 + 1});
    main.push_back({primec::IrOpcode::StoreLocal, k});
  }
  for (uint64_t k = 0; k < Values; ++k) {
    main.push_back({primec::IrOpcode::LoadLocal, Values - 1 - k});
  }
  main.push_back({primec::IrOpcode::Call, 1});
  main.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  for (uint64_t k = 0; k < Values; ++k) {
    main.push_back({primec::IrOpcode::LoadLocal, k});
  }
  main.push_back({primec::IrOpcode::Call, 1});
  main.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  main.push_back({primec::IrOpcode::PushI64, 5});
  main.push_back({primec::IrOpcode::LoadLocal, 3});
  main.push_back({primec::IrOpcode::Call, 2});
  main.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  main.push_back({primec::IrOpcode::CallVoid, 3});
  for (uint64_t k = 0; k < Values; ++k) {
    main.push_back({primec::IrOpcode::LoadLocal, k});
    main.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  }
  main.push_back({primec::IrOpcode::PushI32, 0});
  main.push_back({primec::IrOpcode::ReturnI32, 0});
  // /wide: the sum of parameter k times k + 1 plus its square (squares kept in locals 16..31).
  std::vector<primec::IrInstruction> wide;
  for (uint64_t k = Values; k-- > 0;) {
    wide.push_back({primec::IrOpcode::StoreLocal, k});
  }
  for (uint64_t k = 0; k < Values; ++k) {
    wide.push_back({primec::IrOpcode::LoadLocal, k});
    wide.push_back({primec::IrOpcode::LoadLocal, k});
    wide.push_back({primec::IrOpcode::MulI64, 0});
    wide.push_back({primec::IrOpcode::StoreLocal, Values + k});
  }
  wide.push_back({primec::IrOpcode::PushI64, 0});
  for (uint64_t k = 0; k < Values; ++k) {
    wide.push_back({primec::IrOpcode::LoadLocal, k});
    wide.push_back({primec::IrOpcode::PushI64, k + 1});
    wide.push_back({primec::IrOpcode::MulI64, 0});
    wide.push_back({primec::IrOpcode::AddI64, 0});
    wide.push_back({primec::IrOpcode::LoadLocal, Values + k});
    wide.push_back({primec::IrOpcode::AddI64, 0});
  }
  wide.push_back({primec::IrOpcode::ReturnI64, 0});
  primec::IrModule module;
  module.entryIndex = 0;
  module.functions.push_back(optimizer_test::functionOf("/main", std::move(main)));
  module.functions.push_back(optimizer_test::functionOf("/wide", std::move(wide), Values));
  // /weights(a, b) = a * 1000 + b
  module.functions.push_back(optimizer_test::functionOf("/weights",
                                                        optimizer_test::assemble({"StoreLocal 1",
                                                                                  "StoreLocal 0",
                                                                                  "LoadLocal 0",
                                                                                  "PushI64 1000",
                                                                                  "MulI64",
                                                                                  "LoadLocal 1",
                                                                                  "AddI64",
                                                                                  "ReturnI64"}),
                                                        2));
  // /floats prints weights(7, 2.0 converted to an integer).
  module.functions.push_back(optimizer_test::functionOf("/floats",
                                                        optimizer_test::assemble({"PushI64 7",
                                                                                  "PushI64 1",
                                                                                  "ConvertI64ToF64",
                                                                                  "Dup",
                                                                                  "AddF64",
                                                                                  "ConvertF64ToI64",
                                                                                  "Call 2",
                                                                                  "PrintI64 1",
                                                                                  "ReturnVoid"})));
  expectNativeMatchesVm(module, "regalloc_calls");

  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(module, "regalloc_calls");
  REQUIRE(report.perFunction.size() == 4);
  CHECK(report.perFunction[0].registerAllocated);
  CHECK(report.perFunction[1].registerAllocated);
  CHECK(report.perFunction[2].registerAllocated);
  CHECK(report.perFunction[3].registerAllocated);
}

namespace {

// The instructions of the function that starts after the `skip`th `ret` of `code`, up to and
// including its own `returns`th `ret`, with branch and call targets cut off.
std::vector<std::string>
functionListing(const std::vector<DisassembledInstruction> &code, size_t skip, size_t returns) {
  size_t start = 0;
  for (size_t seen = 0; start < code.size() && seen < skip; ++start) {
    seen += code[start].text == "ret" ? 1 : 0;
  }
  std::vector<std::string> lines;
  size_t seenReturns = 0;
  for (size_t i = start; i < code.size() && seenReturns < returns; ++i) {
    const std::string &text = code[i].text;
    lines.push_back(text.substr(0, std::min(text.size(), text.find(" 0x"))));
    seenReturns += text == "ret" ? 1 : 0;
  }
  return lines;
}

std::string joinLines(const std::vector<std::string> &lines) {
  std::string out;
  for (const std::string &line : lines) {
    out += line + "\n";
  }
  return out;
}

} // namespace

TEST_CASE("arm64 register allocation keeps a float loop in d registers") {
  // sum = 0.0; x = 1.0; while (x < 10.5) { sum = sum + x; x = x + 0.5 }; print (i64)sum
  primec::IrModule module =
      optimizer_test::moduleOf(optimizer_test::assemble({"PushF64 0x0",
                                                         "StoreLocal 0",
                                                         "PushF64 0x3ff0000000000000",
                                                         "StoreLocal 1",
                                                         "LoadLocal 1",
                                                         "PushF64 0x4025000000000000",
                                                         "CmpLtF64",
                                                         "JumpIfZero 17",
                                                         "LoadLocal 0",
                                                         "LoadLocal 1",
                                                         "AddF64",
                                                         "StoreLocal 0",
                                                         "LoadLocal 1",
                                                         "PushF64 0x3fe0000000000000",
                                                         "AddF64",
                                                         "StoreLocal 1",
                                                         "Jump 4",
                                                         "LoadLocal 0",
                                                         "ConvertF64ToI64",
                                                         "PrintI64 1",
                                                         "PushI32 0",
                                                         "ReturnI32"}));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  const Outcome vm = optimizer_test::run(module);
  REQUIRE_MESSAGE(vm.ok, vm.error);
  CHECK(vm.output == "104\n");
  expectNativeMatchesVm(module, "regalloc_float_loop");
  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(module, "regalloc_float_loop");
  REQUIRE(report.perFunction.size() == 1);
  CHECK(report.perFunction[0].registerAllocated);
  CHECK(report.perFunction[0].registerAllocationSpillSlots == 0);

  // Both locals live in d registers from d8 on; the compare is fused into its branch (b.pl: not
  // less, or unordered) and the constants are fmov immediates. Nothing goes through a general
  // register, a frame slot or the value stack inside the loop.
  const std::vector<std::string> code = functionListing(
      disassemble(primec::testing::testScratchPath("native_codegen/regalloc_float_loop.opt")),
      0,
      1);
  INFO(joinLines(code));
  const std::vector<std::string> expected = {"fmov d9, xzr",
                                             "fmov d8, #1.00000000",
                                             "fmov d1, #10.50000000",
                                             "fcmp d8, d1",
                                             "b.pl",
                                             "fadd d9, d9, d8",
                                             "fmov d1, #0.50000000",
                                             "fadd d8, d8, d1",
                                             "b",
                                             "fcvtzs x21, d9"};
  const auto first = std::find(code.begin(), code.end(), expected.front());
  REQUIRE(first != code.end());
  REQUIRE(static_cast<size_t>(code.end() - first) >= expected.size());
  CHECK(std::vector<std::string>(first, first + static_cast<std::ptrdiff_t>(expected.size())) ==
        expected);
}

TEST_CASE("arm64 register allocation keeps floats live across calls and mixes argument kinds") {
  // /main: a = (f64)3, b = a / 8.0; n is printed (a template) while a and b are live, then
  // c = /scale(5, a) with an integer and a float argument; prints (i64)((c + a + b) * 1000).
  // /scale(n, x) = x * (f64)n + 0.25.
  primec::IrModule module;
  module.entryIndex = 0;
  module.functions.push_back(
      optimizer_test::functionOf("/main",
                                 optimizer_test::assemble({"PushI64 3",
                                                           "ConvertI64ToF64",
                                                           "StoreLocal 0",
                                                           "LoadLocal 0",
                                                           "PushF64 0x4020000000000000",
                                                           "DivF64",
                                                           "StoreLocal 1",
                                                           "PushI64 5",
                                                           "PrintI64 1",
                                                           "PushI64 5",
                                                           "LoadLocal 0",
                                                           "Call 1",
                                                           "LoadLocal 0",
                                                           "AddF64",
                                                           "LoadLocal 1",
                                                           "AddF64",
                                                           "PushF64 0x408f400000000000",
                                                           "MulF64",
                                                           "ConvertF64ToI64",
                                                           "PrintI64 1",
                                                           "PushI32 0",
                                                           "ReturnI32"})));
  module.functions.push_back(
      optimizer_test::functionOf("/scale",
                                 optimizer_test::assemble({"StoreLocal 1",
                                                           "StoreLocal 0",
                                                           "LoadLocal 1",
                                                           "LoadLocal 0",
                                                           "ConvertI64ToF64",
                                                           "MulF64",
                                                           "PushF64 0x3fd0000000000000",
                                                           "AddF64",
                                                           "ReturnF64"}),
                                 2));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  const Outcome vm = optimizer_test::run(module);
  REQUIRE_MESSAGE(vm.ok, vm.error);
  CHECK(vm.output == "5\n18625\n");
  expectNativeMatchesVm(module, "regalloc_float_calls");
  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(module, "regalloc_float_calls");
  REQUIRE(report.perFunction.size() == 2);
  CHECK(report.perFunction[0].registerAllocated);
  CHECK(report.perFunction[1].registerAllocated);

  const std::vector<DisassembledInstruction> code =
      disassemble(primec::testing::testScratchPath("native_codegen/regalloc_float_calls.opt"));
  const std::vector<std::string> main = functionListing(code, 0, 1);
  INFO(joinLines(main));
  // a and b live in d9 and d10, which the print template (C-preserved registers) leaves alone:
  // they are saved only around the call, which may change every register, and a goes to the
  // callee in x1 as its bit pattern.
  const auto at = [&](const std::string &text) {
    return static_cast<size_t>(std::find(main.begin(), main.end(), text) - main.begin());
  };
  const size_t call = at("bl");
  REQUIRE(call < main.size());
  size_t stores = 0;
  for (size_t i = 0; i < main.size(); ++i) {
    if (main[i].rfind("str d", 0) == 0) {
      ++stores;
      CHECK(i < call);
      CHECK(i + 5 > call);
    }
  }
  CHECK(stores == 2);
  CHECK(std::find(main.begin(), main.begin() + static_cast<std::ptrdiff_t>(call), "fmov x1, d9") !=
        main.begin() + static_cast<std::ptrdiff_t>(call));
  CHECK(main[call + 1].rfind("fmov d", 0) == 0); // the result, from x0
  CHECK(main[call + 1].find(", x0") != std::string::npos);
  CHECK(main[call + 2].rfind("ldr d", 0) == 0);
  CHECK(main[call + 3].rfind("ldr d", 0) == 0);
  // /scale takes n in x0 and x in x1: n goes home to x21 and converts from there, x moves to a d
  // register, and the result returns in x0 as its bit pattern.
  const std::vector<std::string> scale = functionListing(code, 1, 1);
  INFO(joinLines(scale));
  const std::vector<std::string> body = {"mov x21, x0",
                                         "fmov d9, x1",
                                         "scvtf d8, x21",
                                         "fmul d9, d9, d8",
                                         "fmov d1, #0.25000000",
                                         "fadd d9, d9, d1",
                                         "fmov x0, d9"};
  const auto first = std::find(scale.begin(), scale.end(), body.front());
  REQUIRE(first != scale.end());
  REQUIRE(static_cast<size_t>(scale.end() - first) >= body.size());
  CHECK(std::vector<std::string>(first, first + static_cast<std::ptrdiff_t>(body.size())) == body);
}

TEST_CASE("arm64 float registers match the VM on edge values") {
  // Every comparison of f64 and f32 edge values (zeros of both signs, NaN, infinity), each as a
  // `cset` and fused into a branch, against a register and an immediate operand; arithmetic,
  // negation and conversions on register operands.
  using primec::IrInstruction;
  using primec::IrOpcode;
  const std::vector<uint64_t> doubles = {0x0,
                                         0x8000000000000000u,
                                         0x3ff8000000000000u,  // 1.5
                                         0xc002000000000000u,  // -2.25
                                         0x7ff0000000000000u,  // inf
                                         0x7ff8000000000000u}; // NaN
  const std::vector<uint64_t> floats = {0x0,
                                        0x80000000u,
                                        0x3fc00000u,  // 1.5
                                        0xc0100000u,  // -2.25
                                        0x7f800000u,  // inf
                                        0x7fc00000u}; // NaN
  const std::vector<IrOpcode> doubleCompares = {IrOpcode::CmpEqF64,
                                                IrOpcode::CmpNeF64,
                                                IrOpcode::CmpLtF64,
                                                IrOpcode::CmpLeF64,
                                                IrOpcode::CmpGtF64,
                                                IrOpcode::CmpGeF64};
  const std::vector<IrOpcode> floatCompares = {IrOpcode::CmpEqF32,
                                               IrOpcode::CmpNeF32,
                                               IrOpcode::CmpLtF32,
                                               IrOpcode::CmpLeF32,
                                               IrOpcode::CmpGtF32,
                                               IrOpcode::CmpGeF32};
  std::vector<IrInstruction> body;
  // `value * 1.0` keeps the value (sign, NaN and infinity included) and puts it in a register.
  const auto pushRegister = [&](uint64_t bits, bool isF64) {
    body.push_back({isF64 ? IrOpcode::PushF64 : IrOpcode::PushF32, bits});
    body.push_back(
        {isF64 ? IrOpcode::PushF64 : IrOpcode::PushF32, isF64 ? 0x3ff0000000000000u : 0x3f800000u});
    body.push_back({isF64 ? IrOpcode::MulF64 : IrOpcode::MulF32, 0});
  };
  for (const bool isF64 : {true, false}) {
    const std::vector<uint64_t> &values = isF64 ? doubles : floats;
    for (const uint64_t left : values) {
      for (const uint64_t right : values) {
        for (const IrOpcode op : isF64 ? doubleCompares : floatCompares) {
          for (const bool rightImmediate : {false, true}) {
            for (const bool fused : {false, true}) {
              pushRegister(left, isF64);
              if (rightImmediate) {
                body.push_back({isF64 ? IrOpcode::PushF64 : IrOpcode::PushF32, right});
              } else {
                pushRegister(right, isF64);
              }
              body.push_back({op, 0});
              if (!fused) {
                body.push_back({IrOpcode::PrintI64, primec::PrintFlagNewline});
                continue;
              }
              // JumpIfZero over `print 1` to `print 0`.
              const uint64_t jumpIfZero = body.size();
              body.push_back({IrOpcode::JumpIfZero, jumpIfZero + 4});
              body.push_back({IrOpcode::PushI64, 1});
              body.push_back({IrOpcode::PrintI64, primec::PrintFlagNewline});
              body.push_back({IrOpcode::Jump, jumpIfZero + 6});
              body.push_back({IrOpcode::PushI64, 0});
              body.push_back({IrOpcode::PrintI64, primec::PrintFlagNewline});
            }
          }
        }
      }
    }
  }
  // Arithmetic on finite operands, printed as (i64)(result * 1000) through f64.
  const std::vector<uint64_t> finiteDoubles = {
      0x3ff8000000000000u, 0xc002000000000000u, 0x4008000000000000u, 0x3fe0000000000000u};
  const std::vector<uint64_t> finiteFloats = {0x3fc00000u, 0xc0100000u, 0x40400000u, 0x3f000000u};
  for (const bool isF64 : {true, false}) {
    const std::vector<uint64_t> &values = isF64 ? finiteDoubles : finiteFloats;
    const std::vector<IrOpcode> ops =
        isF64 ? std::vector<IrOpcode>{IrOpcode::AddF64,
                                      IrOpcode::SubF64,
                                      IrOpcode::MulF64,
                                      IrOpcode::DivF64}
              : std::vector<IrOpcode>{
                    IrOpcode::AddF32, IrOpcode::SubF32, IrOpcode::MulF32, IrOpcode::DivF32};
    for (const uint64_t left : values) {
      for (const uint64_t right : values) {
        for (const IrOpcode op : ops) {
          pushRegister(left, isF64);
          pushRegister(right, isF64);
          body.push_back({op, 0});
          body.push_back({isF64 ? IrOpcode::NegF64 : IrOpcode::NegF32, 0});
          if (!isF64) {
            body.push_back({IrOpcode::ConvertF32ToF64, 0});
          }
          body.push_back({IrOpcode::PushF64, 0x408f400000000000u}); // 1000.0
          body.push_back({IrOpcode::MulF64, 0});
          body.push_back({IrOpcode::ConvertF64ToI64, 0});
          body.push_back({IrOpcode::PrintI64, primec::PrintFlagNewline});
        }
      }
    }
  }
  // Conversions: integers to both float widths and back, f64 narrowed to f32, unsigned forms.
  for (const int64_t value : {int64_t{0}, int64_t{7}, int64_t{-123456789}, int64_t{1} << 40}) {
    body.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
    body.push_back({IrOpcode::ConvertI64ToF64, 0});
    body.push_back({IrOpcode::ConvertF64ToF32, 0});
    body.push_back({IrOpcode::ConvertF32ToI64, 0});
    body.push_back({IrOpcode::PrintI64, primec::PrintFlagNewline});
    body.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
    body.push_back({IrOpcode::ConvertI64ToF32, 0});
    body.push_back({IrOpcode::ConvertF32ToF64, 0});
    body.push_back({IrOpcode::ConvertF64ToI64, 0});
    body.push_back({IrOpcode::PrintI64, primec::PrintFlagNewline});
    body.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value) & 0xFFFFFFFFFFFFu});
    body.push_back({IrOpcode::ConvertU64ToF64, 0});
    body.push_back({IrOpcode::ConvertF64ToU64, 0});
    body.push_back({IrOpcode::PrintI64, primec::PrintFlagNewline});
  }
  body.push_back({IrOpcode::PushI32, 0});
  body.push_back({IrOpcode::ReturnI32, 0});
  primec::IrModule module = optimizer_test::moduleOf(std::move(body));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(module, "regalloc_float_edges");
  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(module, "regalloc_float_edges");
  REQUIRE(report.perFunction.size() == 1);
  CHECK(report.perFunction[0].registerAllocated);
}

TEST_CASE("arm64 register allocation emits a recursive function on registers") {
  // fib(n) = n < 2 ? n : fib(n - 1) + fib(n - 2)
  primec::IrModule module;
  module.entryIndex = 0;
  module.functions.push_back(optimizer_test::functionOf(
      "/main",
      optimizer_test::assemble({"PushI32 20", "Call 1", "PrintI32 1", "PushI32 0", "ReturnI32"})));
  module.functions.push_back(optimizer_test::functionOf("/fib",
                                                        optimizer_test::assemble({"StoreLocal 0",
                                                                                  "LoadLocal 0",
                                                                                  "PushI32 2",
                                                                                  "CmpLtI32",
                                                                                  "JumpIfZero 7",
                                                                                  "LoadLocal 0",
                                                                                  "ReturnI32",
                                                                                  "LoadLocal 0",
                                                                                  "PushI32 1",
                                                                                  "SubI32",
                                                                                  "Call 1",
                                                                                  "LoadLocal 0",
                                                                                  "PushI32 2",
                                                                                  "SubI32",
                                                                                  "Call 1",
                                                                                  "AddI32",
                                                                                  "ReturnI32"}),
                                                        1));
  expectNativeMatchesVm(module, "regalloc_fib");
  const primec::NativeEmitterInstrumentation report = arm64AllocationReport(module, "regalloc_fib");
  REQUIRE(report.perFunction.size() == 2);
  CHECK(report.perFunction[0].registerAllocated);
  CHECK(report.perFunction[1].registerAllocated);

  // /fib follows /main, whose code ends at its only `ret`; /fib ends at its second `ret`.
  const std::vector<DisassembledInstruction> code =
      disassemble(primec::testing::testScratchPath("native_codegen/regalloc_fib.opt"));
  size_t start = 0;
  while (start < code.size() && code[start].text != "ret") {
    ++start;
  }
  std::vector<std::string> fib;
  size_t returns = 0;
  for (size_t i = start + 1; i < code.size() && returns < 2; ++i) {
    const std::string &text = code[i].text;
    // Branch targets are addresses; keep the mnemonic only.
    fib.push_back(text.substr(0, std::min(text.size(), text.find(" 0x"))));
    returns += text == "ret" ? 1 : 0;
  }
  std::string listing;
  for (const std::string &line : fib) {
    listing += line + "\n";
  }
  INFO(listing);
  // n arrives in x0 and lives in x23; it is saved in its frame slot across the first call
  // only, and the first result (x22) across the second. Arguments and results pass in x0.
  const std::vector<std::string> expected = {"add x21, x27, #0x0",
                                             "sub sp, sp, #0x230",
                                             "mov x27, sp",
                                             "str x21, [x27, #0x18]",
                                             "str x30, [x27, #0x28]",
                                             "mov x23, x0",
                                             "cmp x23, #0x2",
                                             "b.ge",
                                             "mov x0, x23",
                                             "ldr x30, [x27, #0x28]",
                                             "ldr x27, [x27, #0x18]",
                                             "add sp, sp, #0x230",
                                             "ret",
                                             "sub x21, x23, #0x1",
                                             "str x23, [x27, #0x1a8]",
                                             "mov x0, x21",
                                             "bl",
                                             "mov x22, x0",
                                             "ldr x23, [x27, #0x1a8]",
                                             "sub x23, x23, #0x2",
                                             "str x22, [x27, #0x198]",
                                             "mov x0, x23",
                                             "bl",
                                             "mov x21, x0",
                                             "ldr x22, [x27, #0x198]",
                                             "add x22, x22, x21",
                                             "mov x0, x22",
                                             "ldr x30, [x27, #0x28]",
                                             "ldr x27, [x27, #0x18]",
                                             "add sp, sp, #0x230",
                                             "ret"};
  CHECK(fib == expected);
}

TEST_CASE("arm64 register allocation keeps address-taken locals in their frame slots") {
  // /main takes the address of local 0 and keeps it in local 1. Each of four iterations passes
  // the address to /bump, which doubles the local through it and adds the counter, then reads
  // the local directly into a running sum held in a register, and writes it once more through
  // the address (minus 3) before printing it as a local. Every read of local 0 must see the
  // last write, whichever way it was made.
  primec::IrModule module;
  module.entryIndex = 0;
  module.functions.push_back(optimizer_test::functionOf(
      "/main",
      optimizer_test::assemble({"PushI64 5",   "StoreLocal 0", "AddressOfLocal 0", "StoreLocal 1",
                                "PushI64 0",   "StoreLocal 2", "PushI64 0",        "StoreLocal 3",
                                "LoadLocal 2", "PushI64 4",    "CmpLtI64",         "JumpIfZero 32",
                                "LoadLocal 1", "LoadLocal 2",  "CallVoid 1",       "LoadLocal 3",
                                "LoadLocal 0", "AddI64",       "StoreLocal 3",     "LoadLocal 1",
                                "LoadLocal 0", "PushI64 3",    "SubI64",           "StoreIndirect",
                                "Pop",         "LoadLocal 0",  "PrintI64 1",       "LoadLocal 2",
                                "PushI64 1",   "AddI64",       "StoreLocal 2",     "Jump 8",
                                "LoadLocal 3", "PrintI64 1",   "PushI32 0",        "ReturnI32"})));
  // /bump(address, k): *address = *address * 2 + k
  module.functions.push_back(optimizer_test::functionOf("/bump",
                                                        optimizer_test::assemble({"StoreLocal 1",
                                                                                  "StoreLocal 0",
                                                                                  "LoadLocal 0",
                                                                                  "LoadLocal 0",
                                                                                  "LoadIndirect",
                                                                                  "PushI64 2",
                                                                                  "MulI64",
                                                                                  "LoadLocal 1",
                                                                                  "AddI64",
                                                                                  "StoreIndirect",
                                                                                  "Pop",
                                                                                  "ReturnVoid"}),
                                                        2));
  const Outcome vm = optimizer_test::run(module);
  REQUIRE_MESSAGE(vm.ok, vm.error);
  // 5 -> 10 (sum 10) -> 7; 15 (sum 25) -> 12; 26 (sum 51) -> 23; 49 (sum 100) -> 46
  CHECK(vm.output == "7\n12\n23\n46\n100\n");
  expectNativeMatchesVm(module, "regalloc_address");

  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(module, "regalloc_address");
  REQUIRE(report.perFunction.size() == 2);
  CHECK(report.perFunction[0].registerAllocated);
  CHECK(report.perFunction[1].registerAllocated);
  // The address is x27 plus the slot's offset, built inline.
  const std::vector<DisassembledInstruction> code =
      disassemble(primec::testing::testScratchPath("native_codegen/regalloc_address.opt"));
  bool inlineAddress = false;
  for (const DisassembledInstruction &instruction : code) {
    if (instruction.text.rfind("add x", 0) == 0 &&
        instruction.text.find(", x27, #0x") != std::string::npos) {
      inlineAddress = true;
    }
  }
  CHECK(inlineAddress);
}

TEST_CASE("arm64 register-allocated functions run with two-word conditional branches") {
  // Every function with the far form, as a function too large for b.cond falls back to.
  setenv("PRIMESTRUCT_NATIVE_REGALLOC_FAR_BRANCHES", "1", 1);
  std::vector<primec::IrFunction> bodies;
  for (uint64_t seed = 0; seed < 20; ++seed) {
    bodies.push_back(optimizer_test::makeModule(seed + 61000).functions[0]);
    bodies.back().name = "/program" + std::to_string(seed);
  }
  expectNativeMatchesVm(optimizer_test::driverModule(std::move(bodies)), "regalloc_far");
  expectNativeMatchesVm(optimizer_test::fusedFormsProgram(), "regalloc_far_fused");
  unsetenv("PRIMESTRUCT_NATIVE_REGALLOC_FAR_BRANCHES");
}

TEST_CASE("arm64 register allocation re-emits a function past the reach of b.cond") {
  // A loop whose exit branch jumps over more than 1 MB of code (cbz and b.cond reach +-1 MB): the
  // function is emitted again with two-word branches and still runs. The body runs once (local 0
  // counts down from 1), so the exit condition is a register, not a constant.
  std::vector<primec::IrInstruction> body = optimizer_test::assemble({"PushI64 1",
                                                                      "StoreLocal 0",
                                                                      "PushI64 0",
                                                                      "StoreLocal 1",
                                                                      "LoadLocal 0",
                                                                      "JumpIfZero 0",
                                                                      "LoadLocal 0",
                                                                      "PushI64 1",
                                                                      "SubI64",
                                                                      "StoreLocal 0"});
  const size_t branch = 5;
  for (int i = 0; i < 27000; ++i) {
    // x = x * c1 + c2 with four-word constants: ten words.
    body.push_back({primec::IrOpcode::LoadLocal, 1});
    body.push_back({primec::IrOpcode::PushI64, 0x123456789ABCDEF1ull + static_cast<uint64_t>(i)});
    body.push_back({primec::IrOpcode::MulI64, 0});
    body.push_back({primec::IrOpcode::PushI64, 0x0FEDCBA987654321ull});
    body.push_back({primec::IrOpcode::AddI64, 0});
    body.push_back({primec::IrOpcode::StoreLocal, 1});
  }
  body.push_back({primec::IrOpcode::Jump, 4});
  body[branch].imm = body.size();
  body.push_back({primec::IrOpcode::LoadLocal, 1});
  body.push_back({primec::IrOpcode::PrintI64, primec::PrintFlagNewline});
  body.push_back({primec::IrOpcode::PushI32, 0});
  body.push_back({primec::IrOpcode::ReturnI32, 0});
  primec::IrModule large = optimizer_test::moduleOf(std::move(body));
  large.functions[0].metadata.effectMask = primec::EffectIoOut;
  expectNativeMatchesVm(large, "regalloc_far_large");
  const primec::NativeEmitterInstrumentation report =
      arm64AllocationReport(large, "regalloc_far_large");
  REQUIRE(report.perFunction.size() == 1);
  CHECK(report.perFunction[0].registerAllocated);
  // The exit branch took the two-word form out of range: `cmp`, then `b.ne` over the `b` to the
  // exit (patchCondBranch inverts the condition when the target is beyond b.cond's reach).
  const std::vector<DisassembledInstruction> code =
      disassemble(primec::testing::testScratchPath("native_codegen/regalloc_far_large.opt"));
  bool farExit = false;
  std::string head;
  for (size_t i = 0; i < code.size() && i < 20; ++i) {
    head += code[i].text + "\n";
  }
  INFO(head);
  for (size_t i = 0; i + 2 < code.size() && i < 64; ++i) {
    if (code[i].text.rfind("cmp x", 0) == 0 && code[i].text.find(", xzr") != std::string::npos &&
        code[i + 1].text.rfind("b.ne ", 0) == 0 && code[i + 2].text.rfind("b ", 0) == 0) {
      farExit = true;
    }
  }
  CHECK(farExit);
}
#endif

#else

TEST_CASE("optimized native code is only built on Linux x86_64 and macOS arm64") {
  CHECK(true);
}

#endif

// arm64 constants take the fewest instructions. A small interpreter of the move forms
// (MOVZ, MOVN, MOVK, 32-bit MOVN and ORR from xzr) checks that the words build the value.
namespace {

uint64_t arm64DecodeBitMask(uint32_t n, uint32_t immr, uint32_t imms) {
  const uint32_t combined = (n << 6) | (~imms & 0x3Fu);
  uint32_t length = 6;
  while (length > 0 && (combined & (1u << length)) == 0) {
    --length;
  }
  const uint32_t size = 1u << length;
  const uint32_t levels = size - 1;
  const uint32_t ones = (imms & levels) + 1;
  const uint32_t rotation = immr & levels;
  const uint64_t elementMask = size == 64 ? ~uint64_t{0} : (uint64_t{1} << size) - 1;
  const uint64_t run = ones == 64 ? ~uint64_t{0} : (uint64_t{1} << ones) - 1;
  uint64_t element =
      rotation == 0 ? run : ((run >> rotation) | (run << (size - rotation))) & elementMask;
  uint64_t value = 0;
  for (uint32_t shift = 0; shift < 64; shift += size) {
    value |= element << shift;
  }
  return value;
}

// The value the words leave in x0, or nothing when a word is not one of the move forms.
bool arm64RunMoves(const std::vector<uint32_t> &words, uint64_t &x0) {
  for (const uint32_t word : words) {
    if ((word & 0x1Fu) != 0) {
      return false;
    }
    const uint64_t imm16 = (word >> 5) & 0xFFFFu;
    const uint32_t shift = ((word >> 21) & 0x3u) * 16;
    if ((word & 0xFF800000u) == 0xD2800000u) { // MOVZ x
      x0 = imm16 << shift;
    } else if ((word & 0xFF800000u) == 0x92800000u) { // MOVN x
      x0 = ~(imm16 << shift);
    } else if ((word & 0xFF800000u) == 0xF2800000u) { // MOVK x
      x0 = (x0 & ~(uint64_t{0xFFFF} << shift)) | (imm16 << shift);
    } else if ((word & 0xFFC00000u) == 0x12800000u) { // MOVN w, shift 0 or 16
      x0 = ~(imm16 << shift) & 0xFFFFFFFFu;
    } else if ((word & 0xFF8003E0u) == 0xB20003E0u) { // ORR x, xzr, #bitmask
      x0 = arm64DecodeBitMask((word >> 22) & 1u, (word >> 16) & 0x3Fu, (word >> 10) & 0x3Fu);
    } else {
      return false;
    }
  }
  return true;
}

size_t movWordsNeeded(uint64_t value) {
  size_t zero = 0;
  size_t ones = 0;
  for (int i = 0; i < 4; ++i) {
    const uint64_t half = (value >> (16 * i)) & 0xFFFFu;
    zero += half == 0 ? 1 : 0;
    ones += half == 0xFFFF ? 1 : 0;
  }
  return std::max<size_t>(1, 4 - std::max(zero, ones));
}

} // namespace

TEST_CASE("arm64 constants take one instruction where one encodes them") {
  struct Expected {
    uint64_t value;
    std::vector<uint32_t> words;
  };
  // Encodings from the system assembler.
  const std::vector<Expected> table = {
      {0, {0xD2800000u}},                                // mov x0, #0
      {1, {0xD2800020u}},                                // mov x0, #1
      {~uint64_t{0}, {0x92800000u}},                     // movn x0, #0: -1
      {0xFFFF0000u, {0xD2BFFFE0u}},                      // movz x0, #0xffff, lsl #16
      {0x12345678u, {0xD28ACF00u, 0xF2A24680u}},         // movz; movk lsl #16
      {0x5555555555555555u, {0xB200F3E0u}},              // orr x0, xzr, #0x5555...
      {0x00FF00FF00FF00FFu, {0xB2009FE0u}},              // orr x0, xzr, #0x00ff...
      {0x0000FFFFFFFF0000u, {0xB2707FE0u}},              // orr, not movz and movk
      {0xFFFFFFFFu, {0x12A00000u}},                      // movn w0, #0, lsl #16
      {0xFFFF1234u, {0x129DB960u}},                      // movn w0, #0xedcb
      {static_cast<uint64_t>(-2), {0x92800020u}},        // movn x0, #1
      {0xFFFFFFFF12345678u, {0x929530E0u, 0xF2A24680u}}, // movn x0, #0xa987; movk lsl #16
      {static_cast<uint64_t>(-4096), {0x9281FFE0u}},     // movn x0, #0xfff
      {0x8000000000000000u, {0xD2F00000u}},              // movz x0, #0x8000, lsl #48
      {0x7FFFFFFFFFFFFFFFu, {0x92F00000u}},              // movn x0, #0x8000, lsl #48
  };
  for (const Expected &expected : table) {
    CAPTURE(expected.value);
    CHECK(primec::testing::arm64ConstantWords(0, expected.value) == expected.words);
  }
  // The destination register goes in Rd: mov x26, #42.
  CHECK(primec::testing::arm64ConstantWords(26, 42) == std::vector<uint32_t>{0xD280055Au});
}

TEST_CASE("arm64 constants without a short form keep a full move sequence") {
  // Every half distinct and none 0 or 0xFFFF, and no bitmask: MOVZ plus three MOVKs.
  for (const uint64_t value : {uint64_t{0x123456789ABCDEF0u},
                               uint64_t{0x0123456789ABCDEFu},
                               uint64_t{0xFEDCBA9876543211u}}) {
    CAPTURE(value);
    const std::vector<uint32_t> words = primec::testing::arm64ConstantWords(0, value);
    REQUIRE(words.size() == 4);
    CHECK((words[0] & 0xFF800000u) == 0xD2800000u);
    for (size_t i = 1; i < words.size(); ++i) {
      CHECK((words[i] & 0xFF800000u) == 0xF2800000u);
    }
    uint64_t built = 0;
    REQUIRE(arm64RunMoves(words, built));
    CHECK(built == value);
  }
}

TEST_CASE("arm64 constants build every value in at most the MOVZ or MOVN count") {
  std::vector<uint64_t> values = {0,
                                  1,
                                  ~uint64_t{0},
                                  0xFFFFu,
                                  0x10000u,
                                  0xFFFF0000u,
                                  0xFFFFFFFFu,
                                  0x100000000u,
                                  0x8000000000000000u,
                                  0x7FFFFFFFFFFFFFFFu,
                                  0xFFFF0000FFFF0000u,
                                  0x0000FFFF0000FFFFu,
                                  0xAAAAAAAAAAAAAAAAu,
                                  0x0F0F0F0F0F0F0F0Fu,
                                  0x00000000FFFFFFFEu,
                                  0xFFFFFFFF00000000u};
  uint64_t state = 0x9E3779B97F4A7C15u;
  for (int i = 0; i < 4000; ++i) {
    state = state * 6364136223846793005u + 1442695040888963407u;
    uint64_t value = state;
    // Bias toward the shapes the forms target: clear or set whole halves.
    const uint64_t halves = (state >> 7) & 0xFFu;
    for (int half = 0; half < 4; ++half) {
      const uint64_t mode = (halves >> (2 * half)) & 3u;
      const uint64_t mask = uint64_t{0xFFFF} << (16 * half);
      if (mode == 1) {
        value &= ~mask;
      } else if (mode == 2) {
        value |= mask;
      }
    }
    values.push_back(value);
  }
  for (const uint64_t value : values) {
    CAPTURE(value);
    const std::vector<uint32_t> words = primec::testing::arm64ConstantWords(0, value);
    uint64_t built = 0x5A5A5A5A5A5A5A5Au;
    REQUIRE(arm64RunMoves(words, built));
    CHECK(built == value);
    CHECK(words.size() <= movWordsNeeded(value));
  }
}

TEST_CASE("arm64 stack adjustments take a shifted step and the remainder") {
  using primec::testing::arm64StackAdjustWords;
  CHECK(arm64StackAdjustWords(0, false).empty());
  CHECK(arm64StackAdjustWords(16, false) == std::vector<uint32_t>{0xD10043FFu});
  CHECK(arm64StackAdjustWords(16, true) == std::vector<uint32_t>{0x910043FFu});
  // The entry function's 1 MiB value stack and 16 bytes of locals.
  CHECK(arm64StackAdjustWords(1024 * 1024 + 16, false) ==
        std::vector<uint32_t>{0xD14403FFu,
                              0xD10043FFu}); // sub sp, sp, #256, lsl #12; sub sp, sp, #16
  CHECK(arm64StackAdjustWords(1024 * 1024 + 16, true) ==
        std::vector<uint32_t>{0x914403FFu, 0x910043FFu});
  CHECK(arm64StackAdjustWords(4096, false) == std::vector<uint32_t>{0xD14007FFu});
  // More than 0xFFF pages takes several shifted steps.
  const std::vector<uint32_t> large =
      arm64StackAdjustWords((uint64_t{0xFFF} << 12) + (uint64_t{2} << 12), false);
  CHECK(large == std::vector<uint32_t>{0xD17FFFFFu, 0xD1400BFFu});
}

TEST_CASE("arm64 integer constants become immediate operands of add, sub and compares") {
  using primec::IrOpcode;
  const auto opWord = [](IrOpcode op, uint64_t constant, uint32_t &word) {
    std::vector<uint32_t> words;
    if (!primec::testing::arm64ImmediateOperandWords(op, constant, words)) {
      return false;
    }
    // ldr x0, [x28], #16 (the pop), then the operation.
    REQUIRE(words.size() >= 2);
    word = words[1];
    return true;
  };
  uint32_t word = 0;
  REQUIRE(opWord(IrOpcode::AddI64, 1, word));
  CHECK(word == 0x91000400u); // add x0, x0, #1
  REQUIRE(opWord(IrOpcode::AddI32, static_cast<uint64_t>(-1), word));
  CHECK(word == 0xD1000400u); // sub x0, x0, #1
  REQUIRE(opWord(IrOpcode::SubI64, 1, word));
  CHECK(word == 0xD1000400u); // sub x0, x0, #1
  REQUIRE(opWord(IrOpcode::AddI64, 4 << 12, word));
  CHECK(word == 0x91401000u); // add x0, x0, #4, lsl #12
  REQUIRE(opWord(IrOpcode::CmpLtI64, 10, word));
  CHECK(word == 0xF100281Fu); // cmp x0, #10
  REQUIRE(opWord(IrOpcode::CmpEqI32, static_cast<uint64_t>(-1), word));
  CHECK(word == 0xB100041Fu); // cmn x0, #1
  REQUIRE(opWord(IrOpcode::CmpGeU64, 4096, word));
  CHECK(word == 0xF140041Fu); // cmp x0, #1, lsl #12

  // Adding or subtracting 0 leaves the operand alone: no words at all.
  std::vector<uint32_t> words{1};
  CHECK(primec::testing::arm64ImmediateOperandWords(IrOpcode::AddI64, 0, words));
  CHECK(words.empty());
  CHECK(primec::testing::arm64ImmediateOperandWords(IrOpcode::SubI32, 0, words));
  CHECK(words.empty());

  // A compare's 0 or 1 is one CSET, pushed by a pre-indexed store.
  REQUIRE(primec::testing::arm64ImmediateOperandWords(IrOpcode::CmpGtI32, 0, words));
  CHECK(words == std::vector<uint32_t>{0xF8410780u,   // ldr x0, [x28], #16
                                       0xF100001Fu,   // cmp x0, #0
                                       0x9A9FD7E0u,   // cset x0, gt
                                       0xF81F0F80u}); // str x0, [x28, #-16]!
}

TEST_CASE("arm64 constants without an immediate form stay register operands") {
  using primec::IrOpcode;
  std::vector<uint32_t> words;
  // Neither the constant nor its negation fits 12 bits, plain or shifted.
  CHECK_FALSE(primec::testing::arm64ImmediateOperandWords(IrOpcode::AddI64, 0x1001, words));
  CHECK_FALSE(primec::testing::arm64ImmediateOperandWords(IrOpcode::SubI64, 0x1000001, words));
  CHECK_FALSE(primec::testing::arm64ImmediateOperandWords(IrOpcode::CmpLtI64, 0x123456, words));
  // The most negative value has no CMN form: its negation is itself.
  CHECK_FALSE(
      primec::testing::arm64ImmediateOperandWords(IrOpcode::CmpEqI64, uint64_t{1} << 63, words));
  // Only integer add, sub and compares take immediates.
  CHECK_FALSE(primec::testing::arm64ImmediateOperandWords(IrOpcode::MulI64, 2, words));
  CHECK_FALSE(primec::testing::arm64ImmediateOperandWords(IrOpcode::CmpLtF64, 0, words));
  CHECK_FALSE(primec::testing::arm64ImmediateOperandWords(IrOpcode::DivI64, 1, words));
}

// TODO-5771: the value stack is one pre-indexed store per push and one post-indexed load per pop,
// and a pop that follows its push becomes a register move.
TEST_CASE("arm64 value-stack pushes and pops are one word and fuse into moves") {
  using Step = primec::testing::Arm64ValueStackStep;
  using Kind = Step::Kind;
  // Two pushes popped right after into the template's registers: two moves, no memory.
  CHECK(primec::testing::arm64ValueStackWords(
            {{Kind::Push, 21}, {Kind::Push, 22}, {Kind::Pop, 1}, {Kind::Pop, 15}}) ==
        std::vector<uint32_t>{0x910002C1u,   // add x1, x22, #0
                              0x910002AFu}); // add x15, x21, #0
  // A pop into the pushed register itself leaves nothing.
  CHECK(primec::testing::arm64ValueStackWords({{Kind::Push, 3}, {Kind::Pop, 3}}).empty());
  // A pop with nothing pushed before it in the run loads from memory.
  CHECK(primec::testing::arm64ValueStackWords({{Kind::Pop, 0}}) ==
        std::vector<uint32_t>{0xF8410780u}); // ldr x0, [x28], #16
}

TEST_CASE("arm64 value-stack pushes stay in memory across labels and clobbers") {
  using Step = primec::testing::Arm64ValueStackStep;
  using Kind = Step::Kind;
  // A label between the push and the pop: another path may reach the pop with its own value on
  // the stack, so both stay memory accesses.
  CHECK(
      primec::testing::arm64ValueStackWords({{Kind::Push, 21}, {Kind::Label, 0}, {Kind::Pop, 0}}) ==
      std::vector<uint32_t>{0xF81F0F95u,   // str x21, [x28, #-16]!
                            0xF8410780u}); // ldr x0, [x28], #16
  // Any read of the buffer size counts as a label, not only currentWordIndex(): a template that
  // records an index directly, or a checkpoint, must not see the push removed under it.
  for (const Kind observer : {Kind::CodeSize, Kind::Checkpoint}) {
    CHECK(
        primec::testing::arm64ValueStackWords({{Kind::Push, 21}, {observer, 0}, {Kind::Pop, 0}}) ==
        std::vector<uint32_t>{0xF81F0F95u,   // str x21, [x28, #-16]!
                              0xF8410780u}); // ldr x0, [x28], #16
  }
  // The first pop's move writes x21, the register the earlier push saved: that push stays a store
  // and its pop a load.
  CHECK(primec::testing::arm64ValueStackWords(
            {{Kind::Push, 21}, {Kind::Push, 22}, {Kind::Pop, 21}, {Kind::Pop, 15}}) ==
        std::vector<uint32_t>{0xF81F0F95u,   // str x21, [x28, #-16]!
                              0x910002D5u,   // add x21, x22, #0
                              0xF841078Fu}); // ldr x15, [x28], #16
}

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
// The constant forms and immediate operands run: every add, sub and comparison of edge values with
// edge constants prints what the VM prints.
TEST_CASE("arm64 native constants and immediate operands match the VM") {
  using primec::IrInstruction;
  using primec::IrOpcode;
  const std::vector<uint64_t> lefts = {0,
                                       1,
                                       static_cast<uint64_t>(-1),
                                       4095,
                                       4096,
                                       0x7FFFFFFFFFFFFFFFu,
                                       0x8000000000000000u,
                                       0xFFFFFFFFu,
                                       static_cast<uint64_t>(-4096)};
  const std::vector<uint64_t> constants = {0,
                                           1,
                                           static_cast<uint64_t>(-1),
                                           4095,
                                           4096,
                                           0xFFF000,
                                           0x1001,
                                           static_cast<uint64_t>(-4095),
                                           static_cast<uint64_t>(-4096),
                                           0x8000000000000000u,
                                           0x7FFFFFFFFFFFFFFFu,
                                           0xFFFF0000u,
                                           0x5555555555555555u,
                                           0x123456789ABCDEF0u};
  const std::vector<IrOpcode> ops = {IrOpcode::AddI64,
                                     IrOpcode::SubI64,
                                     IrOpcode::CmpEqI64,
                                     IrOpcode::CmpNeI64,
                                     IrOpcode::CmpLtI64,
                                     IrOpcode::CmpLeI64,
                                     IrOpcode::CmpGtI64,
                                     IrOpcode::CmpGeI64,
                                     IrOpcode::CmpLtU64,
                                     IrOpcode::CmpLeU64,
                                     IrOpcode::CmpGtU64,
                                     IrOpcode::CmpGeU64};
  std::vector<IrInstruction> body;
  for (const uint64_t left : lefts) {
    for (const uint64_t constant : constants) {
      for (const IrOpcode op : ops) {
        body.push_back({IrOpcode::PushI64, left});
        body.push_back({IrOpcode::PushI64, constant});
        body.push_back({op, 0});
        body.push_back({IrOpcode::PrintI64, 1});
      }
    }
  }
  // i32 forms: a sign-extended constant on a sign-extended operand.
  for (const int32_t left : {0, 1, -1, 2147483647, -2147483647 - 1}) {
    for (const int32_t constant : {0, 1, -1, 4095, -4096, 2147483647, -2147483647 - 1}) {
      for (const IrOpcode op :
           {IrOpcode::AddI32, IrOpcode::SubI32, IrOpcode::CmpLtI32, IrOpcode::CmpEqI32}) {
        body.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<uint32_t>(left))});
        body.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<uint32_t>(constant))});
        body.push_back({op, 0});
        body.push_back({IrOpcode::SextI32, 0});
        body.push_back({IrOpcode::PrintI64, 1});
      }
    }
  }
  body.push_back({IrOpcode::PushI32, 7});
  body.push_back({IrOpcode::ReturnI32, 0});
  primec::IrModule module;
  module.entryIndex = 0;
  module.functions.push_back(optimizer_test::functionOf("/main", std::move(body)));

  const optimizer_test::Outcome vm = optimizer_test::run(module);
  REQUIRE_MESSAGE(vm.ok, vm.error);
  const std::filesystem::path binary =
      primec::testing::testScratchPath("native_codegen/arm64_immediates");
  std::filesystem::create_directories(binary.parent_path());
  std::string error;
  REQUIRE_MESSAGE(primec::NativeEmitter().emitExecutable(module, binary.string(), error), error);
  const std::filesystem::path outPath = binary.string() + ".stdout";
  const int status =
      std::system(("'" + binary.string() + "' > '" + outPath.string() + "' 2>/dev/null").c_str());
  CHECK((WIFEXITED(status) ? WEXITSTATUS(status) : -1) == 7);
  std::ifstream file(outPath, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  CHECK(buffer.str() == vm.output);
}
#endif

// TODO-5483: the arm64 backend cannot run here, so its encodings are pinned. SextI32 is SXTW x0,
// w0 (SBFM x0, x0, #0, #31). Float comparisons branch on the fcmp flags with the conditions that
// are false for an unordered pair: EQ, NE (true), MI for <, LS for <=, GT and GE.
TEST_CASE("arm64 SXTW and float comparison results keep their encodings") {
  const std::vector<uint32_t> sext = primec::testing::arm64TemplateWords(primec::IrOpcode::SextI32);
  CHECK(std::find(sext.begin(), sext.end(), 0x93407C00u) != sext.end());

  // The condition of the comparison's CSET (CSINC x0, xzr, xzr with the inverted condition).
  const auto resultCondition = [](primec::IrOpcode op) -> int {
    for (const uint32_t word : primec::testing::arm64TemplateWords(op)) {
      if ((word & 0xFFFF0FFFu) == 0x9A9F07E0u) {
        return static_cast<int>(((word >> 12) & 0xFu) ^ 1u);
      }
    }
    return -1;
  };
  struct Expected {
    primec::IrOpcode op;
    int condition;
  };
  for (const Expected &expected : {Expected{primec::IrOpcode::CmpEqF64, 0x0},
                                   Expected{primec::IrOpcode::CmpNeF64, 0x1},
                                   Expected{primec::IrOpcode::CmpLtF64, 0x4},
                                   Expected{primec::IrOpcode::CmpLeF64, 0x9},
                                   Expected{primec::IrOpcode::CmpGtF64, 0xC},
                                   Expected{primec::IrOpcode::CmpGeF64, 0xA},
                                   Expected{primec::IrOpcode::CmpLtF32, 0x4},
                                   Expected{primec::IrOpcode::CmpLeF32, 0x9}}) {
    CAPTURE(static_cast<int>(expected.op));
    CHECK(resultCondition(expected.op) == expected.condition);
  }
}

// TODO-5582: the Mach-O image can import libSystem functions (dyld binds the pointer slots in its
// writable data segment at load) and keeps zeroed writable state next to them.
TEST_CASE("a Mach-O image imports libSystem functions and keeps writable state") {
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
  const auto runProbe = [](const std::string &symbol, int &signatureStatus) -> int {
    std::vector<uint8_t> image;
    std::string error;
    REQUIRE_MESSAGE(primec::testing::buildMachOImportProbe(symbol, image, error), error);
    const std::filesystem::path binary = primec::testing::testScratchPath("native_codegen/import_probe_" + symbol);
    std::filesystem::create_directories(binary.parent_path());
    {
      std::ofstream out(binary, std::ios::binary);
      out.write(reinterpret_cast<const char *>(image.data()), static_cast<std::streamsize>(image.size()));
    }
    std::filesystem::permissions(binary,
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    const int verify = std::system(("codesign --verify '" + binary.string() + "' > /dev/null 2>&1").c_str());
    signatureStatus = WIFEXITED(verify) ? WEXITSTATUS(verify) : -1;
    const int status = std::system(("'" + binary.string() + "' > /dev/null 2>&1").c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  };
  int signatureStatus = -1;
  // abs(-5) is 5: the pointer slot was bound, the call returned, and the state held the result.
  CHECK(runProbe("abs", signatureStatus) == 5);
  CHECK(signatureStatus == 0);
  // A symbol libSystem does not have stops the program at load, before any of its code runs.
  CHECK(runProbe("primec_no_such_function", signatureStatus) != 5);
#else
  std::vector<uint8_t> image;
  std::string error;
  CHECK_FALSE(primec::testing::buildMachOImportProbe("abs", image, error));
#endif
}
