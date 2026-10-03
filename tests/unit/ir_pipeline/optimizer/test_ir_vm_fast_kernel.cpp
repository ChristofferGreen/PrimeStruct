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

// The loop fuses sequences such as `LoadLocal a; Push c; Cmp; JumpIfZero` into
// single instructions. This module exercises every fused form over operands
// with the signs and widths that tell the comparisons and constant widths apart,
// printing 0/1 for each branch so any wrong fusion shows up as different output.
primec::IrModule fusedFormsModule() {
  using primec::IrInstruction;
  using primec::IrOpcode;
  const std::vector<uint64_t> values = {0,
                                        1,
                                        static_cast<uint64_t>(-1),
                                        5,
                                        static_cast<uint64_t>(-5),
                                        0x7FFFFFFFull,
                                        0x80000000ull,
                                        0x8000000000000000ull,
                                        0x7FFFFFFFFFFFFFFFull};
  const std::vector<IrOpcode> comparisons = {IrOpcode::CmpEqI32,
                                             IrOpcode::CmpNeI32,
                                             IrOpcode::CmpLtI32,
                                             IrOpcode::CmpLeI32,
                                             IrOpcode::CmpGtI32,
                                             IrOpcode::CmpGeI64,
                                             IrOpcode::CmpLtI64};
  std::vector<IrInstruction> code;
  const auto emit = [&](IrOpcode op, uint64_t imm = 0) {
    code.push_back({op, imm});
    return code.size() - 1;
  };
  // Branches print 1 when the comparison holds and 0 otherwise.
  const auto printBranch = [&](size_t jumpIfZeroIndex) {
    emit(IrOpcode::PushI32, 1);
    emit(IrOpcode::PrintI32, primec::PrintFlagNewline);
    const size_t skip = emit(IrOpcode::Jump);
    code[jumpIfZeroIndex].imm = code.size();
    emit(IrOpcode::PushI32, 0);
    emit(IrOpcode::PrintI32, primec::PrintFlagNewline);
    code[skip].imm = code.size();
  };
  for (const uint64_t lhs : values) {
    for (const uint64_t rhs : values) {
      for (const IrOpcode cmp : comparisons) {
        emit(IrOpcode::PushI64, lhs);
        emit(IrOpcode::StoreLocal, 0);
        emit(IrOpcode::PushI64, rhs);
        emit(IrOpcode::StoreLocal, 1);
        // local, constant (PushI32 sign-extends, PushI64 does not)
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::PushI32, rhs);
        emit(cmp);
        printBranch(emit(IrOpcode::JumpIfZero));
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::PushI64, rhs);
        emit(cmp);
        printBranch(emit(IrOpcode::JumpIfZero));
        // local, local
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::LoadLocal, 1);
        emit(cmp);
        printBranch(emit(IrOpcode::JumpIfZero));
        // two stack values from computed operands
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::PushI64, 0);
        emit(IrOpcode::AddI64);
        emit(IrOpcode::LoadLocal, 1);
        emit(cmp);
        printBranch(emit(IrOpcode::JumpIfZero));
      }
      // Arithmetic with a local and a constant or a second local, stored back.
      emit(IrOpcode::PushI64, lhs);
      emit(IrOpcode::StoreLocal, 0);
      emit(IrOpcode::PushI64, rhs);
      emit(IrOpcode::StoreLocal, 1);
      for (const IrOpcode arithmetic : {IrOpcode::AddI64, IrOpcode::SubI32, IrOpcode::MulI64}) {
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::PushI32, rhs);
        emit(arithmetic);
        emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::LoadLocal, 1);
        emit(arithmetic);
        emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      }
      emit(IrOpcode::LoadLocal, 0);
      emit(IrOpcode::PushI32, rhs);
      emit(IrOpcode::AddI32);
      emit(IrOpcode::StoreLocal, 2);
      emit(IrOpcode::LoadLocal, 1);
      emit(IrOpcode::PushI64, lhs);
      emit(IrOpcode::SubI64);
      emit(IrOpcode::StoreLocal, 3);
      emit(IrOpcode::LoadLocal, 2);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      emit(IrOpcode::LoadLocal, 3);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      // Constant stores, copies, the assignment idiom and branch on a local.
      emit(IrOpcode::PushI32, rhs);
      emit(IrOpcode::StoreLocal, 4);
      emit(IrOpcode::LoadLocal, 4);
      emit(IrOpcode::StoreLocal, 5);
      emit(IrOpcode::LoadLocal, 5);
      emit(IrOpcode::PushI64, 3);
      emit(IrOpcode::AddI64);
      emit(IrOpcode::Dup);
      emit(IrOpcode::StoreLocal, 6);
      emit(IrOpcode::Pop);
      emit(IrOpcode::LoadLocal, 6);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      emit(IrOpcode::LoadLocal, 5);
      printBranch(emit(IrOpcode::JumpIfZero));
    }
  }
  emit(IrOpcode::PushI32, 0);
  emit(IrOpcode::ReturnI32);
  primec::IrModule module = optimizer_test::moduleOf(std::move(code));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  return module;
}

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
  const primec::IrModule module = fusedFormsModule();
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
