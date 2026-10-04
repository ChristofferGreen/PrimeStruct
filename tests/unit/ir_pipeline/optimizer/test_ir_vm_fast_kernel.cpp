#include "primec/ir/IrOptimizer.h"
#include "primec/testing/TestScratch.h"
#include "primec/testing/VmKernelSelection.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_random_programs.h"
#include "test_ir_runtime_programs.h"
#include "test_ir_vm_run.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

TEST_SUITE_BEGIN("primestruct.ir.vm_fast_kernel");

// The VM has two execution kernels: the step kernel that debug sessions share,
// and a flat loop for plain runs of modules that pass the shared CFG analysis.
// They must agree on everything observable: result, printed output, and the
// fault message. Every test runs a module on both and compares.

namespace {

using optimizer_test::Outcome;

struct BothKernels {
  Outcome step;
  Outcome fast;
};

BothKernels runBoth(const primec::IrModule &module,
                    const std::vector<std::string_view> &args = {}) {
  BothKernels both;
  primec::testing::setVmFastKernelEnabled(false);
  both.step = optimizer_test::run(module, args);
  primec::testing::setVmFastKernelEnabled(true);
  both.fast = optimizer_test::run(module, args);
  return both;
}

void expectSame(const primec::IrModule &module,
                const std::string &name,
                const std::vector<std::string_view> &args = {}) {
  const BothKernels both = runBoth(module, args);
  INFO(name);
  CHECK(both.fast == both.step);
}

} // namespace

TEST_CASE("the flat loop accepts valid programs and the step kernel keeps the rest") {
  using optimizer_test::moduleOf;
  CHECK(primec::testing::vmFastKernelAccepts(
      moduleOf(optimizer_test::assemble({"PushI32 1", "ReturnI32"}))));
  CHECK(primec::testing::vmFastKernelAccepts(optimizer_test::callsProgram()));
  CHECK(primec::testing::vmFastKernelAccepts(optimizer_test::heapProgram()));

  // Underflow, a return that leaves an operand behind, an entry that takes
  // arguments, and an out-of-range jump are all left to the step kernel.
  CHECK_FALSE(primec::testing::vmFastKernelAccepts(
      moduleOf(optimizer_test::assemble({"AddI64", "ReturnVoid"}))));
  CHECK_FALSE(primec::testing::vmFastKernelAccepts(
      moduleOf(optimizer_test::assemble({"PushI32 1", "PushI32 2", "ReturnI32"}))));
  CHECK_FALSE(
      primec::testing::vmFastKernelAccepts(moduleOf(optimizer_test::assemble({"ReturnVoid"}), 1)));
  CHECK_FALSE(primec::testing::vmFastKernelAccepts(
      moduleOf(optimizer_test::assemble({"Jump 9", "ReturnVoid"}))));
}

TEST_CASE("both kernels agree on random programs, optimized or not") {
  for (uint64_t seed = 0; seed < 300; ++seed) {
    const primec::IrModule module = optimizer_test::makeModule(seed + 77000);
    REQUIRE(primec::testing::vmFastKernelAccepts(module));
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

TEST_CASE("both kernels agree on calls, recursion, heap and indirect addressing") {
  expectSame(optimizer_test::callsProgram(), "calls");
  expectSame(optimizer_test::heapProgram(), "heap");
}

TEST_CASE("both kernels agree on strings, arguments and files") {
  const std::filesystem::path file = primec::testing::testScratchPath("vm_fast_kernel/io.txt");
  const std::vector<std::string_view> args = {"program", "first", "se cond"};
  const primec::IrModule module = optimizer_test::ioProgram(file.string());
  primec::testing::setVmFastKernelEnabled(false);
  const Outcome step = optimizer_test::run(module, args);
  primec::testing::setVmFastKernelEnabled(true);
  const Outcome fast = optimizer_test::run(module, args);
  std::error_code ignored;
  std::filesystem::remove(file, ignored);
  REQUIRE(step.ok);
  CHECK(fast == step);
  CHECK(fast.output.find("first") != std::string::npos);
}

TEST_CASE("both kernels report the same fault for every runtime fault") {
  for (const optimizer_test::FaultProgram &fault : optimizer_test::faultPrograms()) {
    CAPTURE(fault.name);
    const BothKernels both = runBoth(fault.module, {"program"});
    CHECK_FALSE(both.step.ok);
    CHECK(both.fast == both.step);
  }
}

TEST_CASE("faults the flat loop owns match the step kernel") {
  using optimizer_test::moduleOf;
  struct Case {
    const char *name;
    std::vector<const char *> code;
  };
  const std::vector<Case> cases = {
      // Falling off the end and jumping to the end are both a missing return.
      {"fall_off_end", {"PushI32 1", "Pop"}},
      {"jump_to_end", {"Jump 1"}},
      {"empty_function", {}},
      {"divide_min_by_minus_one",
       {"PushI64 9223372036854775808", "PushI64 18446744073709551615", "DivI64", "ReturnI64"}},
      {"float_conversion", {"PushF64 4611686018427387904", "ConvertF64ToI32", "ReturnI32"}},
  };
  for (const Case &testCase : cases) {
    CAPTURE(testCase.name);
    std::vector<primec::IrInstruction> code;
    for (const char *line : testCase.code) {
      code.push_back(optimizer_test::assembleOne(line));
    }
    const primec::IrModule module = moduleOf(std::move(code));
    expectSame(module, testCase.name);
  }
}

TEST_CASE("modules the flat loop does not take still run, with the step kernel's faults") {
  using optimizer_test::moduleOf;
  // Underflow: the step kernel reports it at run time.
  expectSame(moduleOf(optimizer_test::assemble({"AddI64", "ReturnVoid"})), "underflow");
  // A return that leaves an operand behind is fine for the step kernel.
  expectSame(moduleOf(optimizer_test::assemble({"PushI32 1", "PushI32 2", "ReturnI32"})),
             "leftover");
  // An entry function that takes an argument underflows on its first use.
  expectSame(moduleOf(optimizer_test::assemble({"StoreLocal 0", "PushI32 0", "ReturnI32"}), 1),
             "entry argument");
}

TEST_CASE("deep recursion grows the operand stack and locals") {
  using primec::IrOpcode;
  // sum(n) = n == 0 ? 0 : n + sum(n - 1); each frame keeps one local and one
  // pending operand, so a depth of 3000 forces both arenas to grow.
  primec::IrModule module;
  module.entryIndex = 0;
  module.functions.push_back(optimizer_test::functionOf(
      "/main",
      optimizer_test::assemble(
          {"PushI32 3000", "Call 1", "PrintI32 1", "PushI32 0", "ReturnI32"})));
  module.functions.push_back(optimizer_test::functionOf("/sum",
                                                        optimizer_test::assemble({"StoreLocal 0",
                                                                                  "LoadLocal 0",
                                                                                  "JumpIfZero 12",
                                                                                  "LoadLocal 0",
                                                                                  "LoadLocal 0",
                                                                                  "PushI32 1",
                                                                                  "SubI32",
                                                                                  "Call 1",
                                                                                  "AddI32",
                                                                                  "ReturnI32",
                                                                                  "PushI32 0",
                                                                                  "ReturnI32",
                                                                                  "PushI32 0",
                                                                                  "ReturnI32"}),
                                                        1));
  const BothKernels both = runBoth(module);
  REQUIRE(both.step.ok);
  CHECK(both.fast == both.step);
  CHECK(both.fast.output == "4501500\n");
}

namespace {

// `LoadLocal; Push; Cmp; JumpIfZero` where a branch lands on the Push: the
// sequence must not be fused because two paths reach its middle.
primec::IrModule jumpIntoSequenceModule(uint64_t selector) {
  primec::IrModule module = optimizer_test::moduleOf(optimizer_test::assemble(
      {"PushI32 0",   "StoreLocal 2",  "PushI32 5",   "StoreLocal 0", "PushI32 -2",  "StoreLocal 1",
       "LoadLocal 2", "JumpIfZero 10", "LoadLocal 0", "Jump 11",      "LoadLocal 1", "PushI32 3",
       "CmpLtI32",    "JumpIfZero 17", "PushI32 111", "PrintI32 1",   "Jump 19",     "PushI32 222",
       "PrintI32 1",  "PushI32 0",     "ReturnI32"}));
  module.functions[0].instructions[0].imm = selector;
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  return module;
}

} // namespace

TEST_CASE("fused instruction forms agree with the step kernel over edge-case operands") {
  const primec::IrModule module = optimizer_test::fusedFormsProgram();
  REQUIRE(primec::testing::vmFastKernelAccepts(module));
  const BothKernels both = runBoth(module);
  REQUIRE(both.step.ok);
  CHECK(both.fast == both.step);
  CHECK(both.fast.output.size() > 10000);
}

TEST_CASE("fusion never spans a jump target") {
  for (const uint64_t selector : {0u, 1u}) {
    CAPTURE(selector);
    const primec::IrModule module = jumpIntoSequenceModule(selector);
    REQUIRE(primec::testing::vmFastKernelAccepts(module));
    const BothKernels both = runBoth(module);
    REQUIRE(both.step.ok);
    CHECK(both.fast == both.step);
    CHECK(both.fast.output == (selector == 0 ? "111\n" : "222\n"));
  }
}

TEST_CASE("fused string byte loads match the step kernel, including their faults") {
  using optimizer_test::assembleOne;
  using optimizer_test::moduleOf;
  for (const char *position : {"0", "1", "2", "3", "18446744073709551615"}) {
    for (const char *stringIndex : {"0", "5"}) {
      CAPTURE(position);
      CAPTURE(stringIndex);
      // LoadLocal; LoadStringByte; StoreLocal and LoadLocal; LoadStringByte.
      for (const bool store : {true, false}) {
        std::vector<std::string> lines = {std::string("PushI64 ") + position,
                                          "StoreLocal 0",
                                          "LoadLocal 0",
                                          std::string("LoadStringByte ") + stringIndex};
        if (store) {
          lines.insert(lines.end(), {"StoreLocal 1", "LoadLocal 1"});
        }
        lines.push_back("ReturnI32");
        std::vector<primec::IrInstruction> code;
        for (const std::string &line : lines) {
          code.push_back(assembleOne(line.c_str()));
        }
        primec::IrModule module = moduleOf(std::move(code));
        module.stringTable = {"abc"};
        REQUIRE(primec::testing::vmFastKernelAccepts(module));
        expectSame(module, store ? "store form" : "push form");
      }
    }
  }
}

TEST_CASE("fused sext forms match the step kernel over wrapping operands") {
  using optimizer_test::assembleOne;
  using optimizer_test::moduleOf;
  const std::vector<const char *> values = {
      "0", "1", "2147483647", "2147483648", "4294967295", "4294967296", "18446744073709551615"};
  struct Form {
    const char *name;
    std::vector<std::string> body; // uses locals 0 and 1; leaves one value
  };
  const std::vector<Form> forms = {
      {"add_imm", {"LoadLocal 0", "PushI32 1", "AddI32", "SextI32"}},
      {"sub_imm", {"LoadLocal 0", "PushI32 1", "SubI32", "SextI32"}},
      {"mul_imm", {"LoadLocal 0", "PushI32 65537", "MulI32", "SextI32"}},
      {"add_local", {"LoadLocal 0", "LoadLocal 1", "AddI32", "SextI32"}},
      {"sub_local", {"LoadLocal 0", "LoadLocal 1", "SubI32", "SextI32"}},
      {"mul_local", {"LoadLocal 0", "LoadLocal 1", "MulI32", "SextI32"}},
      {"add_imm_store",
       {"LoadLocal 0", "PushI32 1", "AddI32", "SextI32", "StoreLocal 2", "LoadLocal 2"}},
      {"sub_imm_store",
       {"LoadLocal 0", "PushI32 1", "SubI32", "SextI32", "StoreLocal 2", "LoadLocal 2"}},
  };
  for (const Form &form : forms) {
    for (const char *first : values) {
      for (const char *second : values) {
        CAPTURE(form.name);
        CAPTURE(first);
        CAPTURE(second);
        std::vector<std::string> lines = {std::string("PushI64 ") + first,
                                          "StoreLocal 0",
                                          std::string("PushI64 ") + second,
                                          "StoreLocal 1"};
        lines.insert(lines.end(), form.body.begin(), form.body.end());
        lines.push_back("PrintI64 1");
        lines.push_back("PushI32 0");
        lines.push_back("ReturnI32");
        std::vector<primec::IrInstruction> code;
        for (const std::string &line : lines) {
          code.push_back(assembleOne(line.c_str()));
        }
        primec::IrModule module = moduleOf(std::move(code));
        module.functions[0].metadata.effectMask = primec::EffectIoOut;
        REQUIRE(primec::testing::vmFastKernelAccepts(module));
        const BothKernels both = runBoth(module);
        REQUIRE(both.step.ok);
        CHECK(both.fast == both.step);
      }
    }
  }
}

namespace {

struct SinkChunks {
  std::vector<std::pair<int, std::string>> chunks;
};

void recordChunk(int fd, std::string_view chunk, void *userData) {
  static_cast<SinkChunks *>(userData)->chunks.emplace_back(fd, std::string(chunk));
}

} // namespace

TEST_CASE("the output sink receives stdout and stderr bytes in program order on both kernels") {
  using primec::IrInstruction;
  using primec::IrOpcode;
  primec::IrModule module = optimizer_test::moduleOf({});
  module.stringTable = {"text"};
  module.functions[0].instructions = {
      {IrOpcode::PushI32, 7},
      {IrOpcode::PrintI32, primec::encodePrintFlags(true, false)},
      {IrOpcode::PushI32, 8},
      {IrOpcode::PrintI32, primec::encodePrintFlags(false, true)},
      {IrOpcode::PrintString,
       primec::encodePrintStringImm(0, primec::encodePrintFlags(true, true))},
      {IrOpcode::PushI32, 1}, // file handle 1 is stdout
      {IrOpcode::FileWriteNewline, 0},
      {IrOpcode::Pop, 0},
      {IrOpcode::PushI32, 2}, // file handle 2 is stderr
      {IrOpcode::FileWriteString, 0},
      {IrOpcode::Pop, 0},
      {IrOpcode::PushI32, 0},
      {IrOpcode::ReturnI32, 0},
  };
  module.functions[0].metadata.effectMask = primec::EffectIoOut | primec::EffectIoErr;
  for (const bool fast : {false, true}) {
    CAPTURE(fast);
    primec::testing::setVmFastKernelEnabled(fast);
    SinkChunks sink;
    primec::Vm vm;
    vm.setOutputSink({&recordChunk, &sink});
    uint64_t result = 99;
    std::string error;
    REQUIRE_MESSAGE(vm.execute(module, result, error), error);
    CHECK(result == 0);
    const std::vector<std::pair<int, std::string>> expected = {
        {1, "7\n"}, {2, "8"}, {2, "text\n"}, {1, "\n"}, {2, "text"}};
    CHECK(sink.chunks == expected);

    // Clearing the sink restores the process streams: nothing more reaches the old sink.
    vm.clearOutputSink();
    SinkChunks after;
    vm.setOutputSink({&recordChunk, &after});
    vm.clearOutputSink();
    uint64_t again = 0;
    REQUIRE(
        vm.execute(optimizer_test::moduleOf(optimizer_test::assemble({"PushI32 3", "ReturnI32"})),
                   again,
                   error));
    CHECK(after.chunks.empty());
    CHECK(again == 3);
  }
  primec::testing::setVmFastKernelEnabled(true);
}
