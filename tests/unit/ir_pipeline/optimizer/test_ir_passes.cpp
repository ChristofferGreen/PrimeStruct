#include "primec/ir/IrOptimizer.h"
#include "primec/ir/IrPreparation.h"
#include "primec/ir/IrValidation.h"

#include "test_ir_optimizer_helpers.h"

#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.ir.passes");

using optimizer_test::assemble;
using optimizer_test::listing;
using optimizer_test::moduleOf;

namespace {

struct Golden {
  const char *name;
  std::vector<primec::IrInstruction> before;
  std::vector<primec::IrInstruction> after;
  uint32_t parameterCount = 0;
};

// Runs exactly one pass (level 0 plus --opt-pass=<name>) with per-pass
// verification and checks the resulting instruction list.
void expectGolden(const char *passName,
                  const Golden &golden,
                  primec::IrValidationTarget target = primec::IrValidationTarget::Any) {
  CAPTURE(golden.name);
  primec::IrModule module = moduleOf(golden.before, golden.parameterCount);
  const primec::IrModule expected = moduleOf(golden.after, golden.parameterCount);

  primec::OptimizationOptions options;
  options.enabledPasses = {passName};
  options.verifyEachPass = true;
  primec::IrOptimizationReport report;
  std::string error;
  REQUIRE_MESSAGE(primec::optimizeIrModule(module, options, target, report, error), error);
  CHECK_MESSAGE(listing(module) == listing(expected),
                "\nactual:\n",
                listing(module),
                "expected:\n",
                listing(expected));
  CHECK_MESSAGE(primec::verifyIrModuleForOptimization(module, target, error), error);
}

} // namespace

TEST_CASE("cfg-simplify folds constant branches") {
  expectGolden(
      "cfg-simplify",
      {"constant true falls through and drops the dead arm",
       assemble({"PushI32 1", "JumpIfZero 4", "PushI32 7", "ReturnI32", "PushI32 9", "ReturnI32"}),
       assemble({"PushI32 7", "ReturnI32"})});
  expectGolden(
      "cfg-simplify",
      {"constant false always jumps",
       assemble({"PushI32 0", "JumpIfZero 4", "PushI32 7", "ReturnI32", "PushI32 9", "ReturnI32"}),
       assemble({"PushI32 9", "ReturnI32"})});
  expectGolden("cfg-simplify",
               {"64-bit constants too",
                assemble({"PushI64 0", "JumpIfZero 3", "PushI32 1", "PushI32 2", "ReturnI32"}),
                assemble({"PushI32 2", "ReturnI32"})});
  expectGolden(
      "cfg-simplify",
      {"a non-constant condition is left alone",
       assemble(
           {"LoadLocal 0", "JumpIfZero 4", "PushI32 7", "ReturnI32", "PushI32 9", "ReturnI32"}),
       assemble(
           {"LoadLocal 0", "JumpIfZero 4", "PushI32 7", "ReturnI32", "PushI32 9", "ReturnI32"})});
  // `a && b` lowered with a shared test: the arm that pushed a constant already
  // knows the outcome and jumps straight to it; the test stays for the arm that
  // pushed a real value.
  expectGolden("cfg-simplify",
               {"a constant pushed into a join-point branch jumps to the outcome",
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "LoadLocal 1",
                          "Jump 5",
                          "PushI32 0",
                          "JumpIfZero 8",
                          "PushI32 3",
                          "ReturnI32",
                          "PushI32 4",
                          "ReturnI32"}),
                assemble({"LoadLocal 0",
                          "JumpIfZero 6",
                          "LoadLocal 1",
                          "JumpIfZero 6",
                          "PushI32 3",
                          "ReturnI32",
                          "PushI32 4",
                          "ReturnI32"})});
  expectGolden("cfg-simplify",
               {"a nonzero constant pushed into a join-point branch skips the test",
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "LoadLocal 1",
                          "Jump 5",
                          "PushI32 1",
                          "JumpIfZero 8",
                          "PushI32 3",
                          "ReturnI32",
                          "PushI32 4",
                          "ReturnI32"}),
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "LoadLocal 1",
                          "JumpIfZero 6",
                          "PushI32 3",
                          "ReturnI32",
                          "PushI32 4",
                          "ReturnI32"})});
}

TEST_CASE("cfg-simplify threads jumps and drops no-op jumps") {
  expectGolden("cfg-simplify",
               {"jump to a jump goes straight to the final target",
                assemble({"Jump 2", "PushI32 1", "Jump 4", "PushI32 2", "PushI32 5", "ReturnI32"}),
                assemble({"PushI32 5", "ReturnI32"})});
  expectGolden("cfg-simplify",
               {"a conditional jump to the next instruction still pops its condition",
                assemble({"LoadLocal 0", "JumpIfZero 2", "PushI32 1", "ReturnI32"}),
                assemble({"LoadLocal 0", "Pop", "PushI32 1", "ReturnI32"})});
  expectGolden("cfg-simplify",
               {"a jump to the next instruction disappears",
                assemble({"PushI32 1", "Jump 2", "ReturnI32"}),
                assemble({"PushI32 1", "ReturnI32"})});
  expectGolden("cfg-simplify",
               {"a self-loop is not threaded forever",
                assemble({"Jump 1", "Jump 1"}),
                assemble({"Jump 0"})});
}

TEST_CASE("cfg-simplify removes unreachable code and keeps targets right") {
  expectGolden("cfg-simplify",
               {"code after a return",
                assemble({"PushI32 1", "ReturnI32", "PushI32 2", "ReturnI32"}),
                assemble({"PushI32 1", "ReturnI32"})});
  expectGolden("cfg-simplify",
               {"a loop with a dead tail keeps its back edge",
                assemble({"LoadLocal 0",
                          "JumpIfZero 5",
                          "PushI32 1",
                          "StoreLocal 0",
                          "Jump 0",
                          "PushI32 0",
                          "ReturnI32",
                          "PushI32 8",
                          "ReturnI32"}),
                assemble({"LoadLocal 0",
                          "JumpIfZero 5",
                          "PushI32 1",
                          "StoreLocal 0",
                          "Jump 0",
                          "PushI32 0",
                          "ReturnI32"})});
}

TEST_CASE("const-fold evaluates integer arithmetic and comparisons") {
  expectGolden("const-fold",
               {"add",
                assemble({"PushI32 2", "PushI32 3", "AddI32", "ReturnI32"}),
                assemble({"PushI32 5", "ReturnI32"})});
  expectGolden("const-fold",
               {"chain folds bottom-up",
                assemble({"PushI32 2", "PushI32 3", "AddI32", "PushI32 4", "MulI32", "ReturnI32"}),
                assemble({"PushI32 20", "ReturnI32"})});
  expectGolden("const-fold",
               {"sub and neg",
                assemble({"PushI32 10", "PushI32 3", "SubI32", "NegI32", "ReturnI32"}),
                assemble({"PushI32 -7", "ReturnI32"})});
  expectGolden("const-fold",
               {"i64 wraps like the VM",
                assemble({"PushI64 9223372036854775807", "PushI64 1", "AddI64", "ReturnI64"}),
                assemble({"PushI64 -9223372036854775808", "ReturnI64"})});
  expectGolden("const-fold",
               {"signed and unsigned compare",
                assemble({"PushI32 -1",
                          "PushI32 1",
                          "CmpLtI32",
                          "PushI64 -1",
                          "PushI64 1",
                          "CmpLtU64",
                          "AddI32",
                          "ReturnI32"}),
                assemble({"PushI32 1", "ReturnI32"})});
  expectGolden("const-fold",
               {"division by a nonzero constant folds",
                assemble({"PushI32 7", "PushI32 2", "DivI32", "ReturnI32"}),
                assemble({"PushI32 3", "ReturnI32"})});
}

TEST_CASE("const-fold leaves unsafe or unprofitable cases alone") {
  expectGolden("const-fold",
               {"division by zero must still fault at run time",
                assemble({"PushI32 7", "PushI32 0", "DivI32", "ReturnI32"}),
                assemble({"PushI32 7", "PushI32 0", "DivI32", "ReturnI32"})});
  expectGolden("const-fold",
               {"an i32 result outside 32 bits is not baked in",
                assemble({"PushI32 2147483647", "PushI32 1", "AddI32", "ReturnI32"}),
                assemble({"PushI32 2147483647", "PushI32 1", "AddI32", "ReturnI32"})});
  expectGolden("const-fold",
               {"a join point stops folding across it",
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "PushI32 1",
                          "Jump 5",
                          "PushI32 1",
                          "PushI32 2",
                          "AddI32",
                          "ReturnI32"}),
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "PushI32 1",
                          "Jump 5",
                          "PushI32 1",
                          "PushI32 2",
                          "AddI32",
                          "ReturnI32"})});
  expectGolden(
      "const-fold",
      {"a push that feeds a dup is not erased with a later fold",
       assemble({"PushI32 1", "Dup", "StoreLocal 0", "PushI32 0", "CmpNeI32", "ReturnI32"}),
       assemble({"PushI32 1", "Dup", "StoreLocal 0", "PushI32 0", "CmpNeI32", "ReturnI32"})});
  expectGolden("const-fold",
               {"an unknown operand blocks the fold",
                assemble({"LoadLocal 0", "PushI32 2", "AddI32", "ReturnI32"}),
                assemble({"LoadLocal 0", "PushI32 2", "AddI32", "ReturnI32"})});
  expectGolden("const-fold",
               {"operands separated by a balanced no-op still fold",
                assemble({"PushI32 2", "LoadLocal 0", "Pop", "PushI32 3", "AddI32", "ReturnI32"}),
                assemble({"LoadLocal 0", "Pop", "PushI32 5", "ReturnI32"})});
}

TEST_CASE("const-fold evaluates floats and conversions but never bakes in a NaN") {
  expectGolden("const-fold",
               {"f32 add",
                assemble({"PushF32 0x3fc00000", "PushF32 0x40200000", "AddF32", "ReturnF32"}),
                assemble({"PushF32 0x40800000", "ReturnF32"})});
  expectGolden(
      "const-fold",
      {"f64 compare",
       assemble(
           {"PushF64 0x3ff0000000000000", "PushF64 0x4000000000000000", "CmpLtF64", "ReturnI32"}),
       assemble({"PushI32 1", "ReturnI32"})});
  expectGolden("const-fold",
               {"int to float",
                assemble({"PushI32 5", "ConvertI32ToF64", "ReturnF64"}),
                assemble({"PushF64 0x4014000000000000", "ReturnF64"})});
  expectGolden("const-fold",
               {"in-range float to int",
                assemble({"PushF64 0x4004000000000000", "ConvertF64ToI32", "ReturnI32"}),
                assemble({"PushI32 2", "ReturnI32"})});
  expectGolden("const-fold",
               {"0/0 would be a host-dependent NaN",
                assemble({"PushF32 0x0", "PushF32 0x0", "DivF32", "ReturnF32"}),
                assemble({"PushF32 0x0", "PushF32 0x0", "DivF32", "ReturnF32"})});
  expectGolden("const-fold",
               {"out-of-range float to int is host-defined",
                assemble({"PushF64 0x4202a05f20000000", "ConvertF64ToI32", "ReturnI32"}),
                assemble({"PushF64 0x4202a05f20000000", "ConvertF64ToI32", "ReturnI32"})});
}

TEST_CASE("peephole removes dead pushes and identities") {
  expectGolden("peephole",
               {"push then pop",
                assemble({"PushI32 1", "Pop", "PushI32 2", "ReturnI32"}),
                assemble({"PushI32 2", "ReturnI32"})});
  expectGolden("peephole",
               {"dup then pop",
                assemble({"LoadLocal 0", "Dup", "Pop", "ReturnI32"}),
                assemble({"LoadLocal 0", "ReturnI32"})});
  expectGolden("peephole",
               {"unused unary result",
                assemble({"LoadLocal 0", "NegI32", "Pop", "ReturnVoid"}),
                assemble({"ReturnVoid"})});
  expectGolden("peephole",
               {"unused binary result",
                assemble({"LoadLocal 0", "LoadLocal 1", "AddI32", "Pop", "ReturnVoid"}),
                assemble({"ReturnVoid"})});
  expectGolden("peephole",
               {"a division may fault so it stays",
                assemble({"LoadLocal 0", "LoadLocal 1", "DivI32", "Pop", "ReturnVoid"}),
                assemble({"LoadLocal 0", "LoadLocal 1", "DivI32", "Pop", "ReturnVoid"})});
  expectGolden("peephole",
               {"calls and stores are never dropped",
                assemble({"AddressOfLocal 0", "PushI32 1", "StoreIndirect", "Pop", "ReturnVoid"}),
                assemble({"AddressOfLocal 0", "PushI32 1", "StoreIndirect", "Pop", "ReturnVoid"})});
}

TEST_CASE("peephole collapses the assignment-statement idiom") {
  expectGolden(
      "peephole",
      {"dup, store, pop is a store",
       assemble(
           {"LoadLocal 0", "LoadLocal 1", "AddI64", "Dup", "StoreLocal 2", "Pop", "ReturnVoid"}),
       assemble({"LoadLocal 0", "LoadLocal 1", "AddI64", "StoreLocal 2", "ReturnVoid"})});
  expectGolden(
      "peephole",
      {"several in a row",
       assemble({"PushI64 1",
                 "Dup",
                 "StoreLocal 0",
                 "Pop",
                 "PushI64 2",
                 "Dup",
                 "StoreLocal 1",
                 "Pop",
                 "ReturnVoid"}),
       assemble({"PushI64 1", "StoreLocal 0", "PushI64 2", "StoreLocal 1", "ReturnVoid"})});
  expectGolden("peephole",
               {"the result is still used, so the dup stays",
                assemble({"PushI64 1", "Dup", "StoreLocal 0", "ReturnI64"}),
                assemble({"PushI64 1", "Dup", "StoreLocal 0", "ReturnI64"})});
  // The pop is entered from two paths, one of which skips the dup and store,
  // so the pop cannot be removed together with them.
  expectGolden("peephole",
               {"a join point at the pop blocks the rewrite",
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "PushI32 1",
                          "Jump 7",
                          "PushI32 5",
                          "Dup",
                          "StoreLocal 1",
                          "Pop",
                          "ReturnVoid"}),
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "PushI32 1",
                          "Jump 7",
                          "PushI32 5",
                          "Dup",
                          "StoreLocal 1",
                          "Pop",
                          "ReturnVoid"})});
}

TEST_CASE("peephole simplifies arithmetic identities and double negation") {
  expectGolden("peephole",
               {"x + 0",
                assemble({"LoadLocal 0", "PushI32 0", "AddI32", "ReturnI32"}),
                assemble({"LoadLocal 0", "ReturnI32"})});
  expectGolden("peephole",
               {"x - 0",
                assemble({"LoadLocal 0", "PushI64 0", "SubI64", "ReturnI64"}),
                assemble({"LoadLocal 0", "ReturnI64"})});
  expectGolden("peephole",
               {"x * 1",
                assemble({"LoadLocal 0", "PushI32 1", "MulI32", "ReturnI32"}),
                assemble({"LoadLocal 0", "ReturnI32"})});
  expectGolden("peephole",
               {"x / 1",
                assemble({"LoadLocal 0", "PushI32 1", "DivI32", "ReturnI32"}),
                assemble({"LoadLocal 0", "ReturnI32"})});
  expectGolden("peephole",
               {"0 - x is not an identity",
                assemble({"PushI32 0", "LoadLocal 0", "SubI32", "ReturnI32"}),
                assemble({"PushI32 0", "LoadLocal 0", "SubI32", "ReturnI32"})});
  expectGolden("peephole",
               {"x * 0 is not rewritten",
                assemble({"LoadLocal 0", "PushI32 0", "MulI32", "ReturnI32"}),
                assemble({"LoadLocal 0", "PushI32 0", "MulI32", "ReturnI32"})});
  expectGolden("peephole",
               {"double negation",
                assemble({"LoadLocal 0", "NegI64", "NegI64", "ReturnI64"}),
                assemble({"LoadLocal 0", "ReturnI64"})});
  expectGolden("peephole",
               {"mixed-width negations stay",
                assemble({"LoadLocal 0", "NegI32", "NegI64", "ReturnI64"}),
                assemble({"LoadLocal 0", "NegI32", "NegI64", "ReturnI64"})});
  expectGolden("peephole",
               {"float x + 0.0 is not x (negative zero)",
                assemble({"LoadLocal 0", "PushF64 0x0", "AddF64", "ReturnF64"}),
                assemble({"LoadLocal 0", "PushF64 0x0", "AddF64", "ReturnF64"})});
}

TEST_CASE("peephole never pairs instructions across a join point") {
  // The Pop at index 5 is entered from two paths, so the push before it is not
  // necessarily the value it pops.
  expectGolden("peephole",
               {"join point at the pop",
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "PushI32 9",
                          "Jump 5",
                          "PushI32 8",
                          "Pop",
                          "ReturnVoid"}),
                assemble({"LoadLocal 0",
                          "JumpIfZero 4",
                          "PushI32 9",
                          "Jump 5",
                          "PushI32 8",
                          "Pop",
                          "ReturnVoid"})});
  // A jump into the removed pair lands on whatever follows.
  expectGolden(
      "peephole",
      {"jump into an erased pair",
       assemble({"LoadLocal 0", "JumpIfZero 3", "ReturnVoid", "PushI32 1", "Pop", "ReturnVoid"}),
       assemble({"LoadLocal 0", "JumpIfZero 3", "ReturnVoid", "ReturnVoid"})});
}

TEST_CASE("dead-store turns unread stores into pops") {
  expectGolden("dead-store",
               {"never read",
                assemble({"PushI32 1", "StoreLocal 0", "PushI32 2", "ReturnI32"}),
                assemble({"PushI32 1", "Pop", "PushI32 2", "ReturnI32"})});
  expectGolden("dead-store",
               {"read keeps the store",
                assemble({"PushI32 1", "StoreLocal 0", "LoadLocal 0", "ReturnI32"}),
                assemble({"PushI32 1", "StoreLocal 0", "LoadLocal 0", "ReturnI32"})});
  expectGolden(
      "dead-store",
      {"overwritten before any read",
       assemble(
           {"PushI32 1", "StoreLocal 0", "PushI32 2", "StoreLocal 0", "LoadLocal 0", "ReturnI32"}),
       assemble({"PushI32 1", "Pop", "PushI32 2", "StoreLocal 0", "LoadLocal 0", "ReturnI32"})});
  expectGolden("dead-store",
               {"live on one branch only",
                assemble({"PushI32 1",
                          "StoreLocal 0",
                          "LoadLocal 1",
                          "JumpIfZero 6",
                          "LoadLocal 0",
                          "ReturnI32",
                          "PushI32 0",
                          "ReturnI32"}),
                assemble({"PushI32 1",
                          "StoreLocal 0",
                          "LoadLocal 1",
                          "JumpIfZero 6",
                          "LoadLocal 0",
                          "ReturnI32",
                          "PushI32 0",
                          "ReturnI32"})});
  expectGolden("dead-store",
               {"a store read by the next loop iteration is live",
                assemble({"PushI32 3",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "JumpIfZero 9",
                          "LoadLocal 0",
                          "PushI32 1",
                          "SubI32",
                          "StoreLocal 0",
                          "Jump 2",
                          "PushI32 0",
                          "ReturnI32"}),
                assemble({"PushI32 3",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "JumpIfZero 9",
                          "LoadLocal 0",
                          "PushI32 1",
                          "SubI32",
                          "StoreLocal 0",
                          "Jump 2",
                          "PushI32 0",
                          "ReturnI32"})});
  expectGolden("dead-store",
               {"locals whose address is taken are never touched",
                assemble({"PushI32 1", "StoreLocal 0", "AddressOfLocal 0", "Pop", "ReturnVoid"}),
                assemble({"PushI32 1", "StoreLocal 0", "AddressOfLocal 0", "Pop", "ReturnVoid"})});
  expectGolden("dead-store",
               {"an unused parameter is still popped",
                assemble({"StoreLocal 0", "ReturnVoid"}),
                assemble({"Pop", "ReturnVoid"}),
                1});
}

TEST_CASE("the -O1 pipeline cleans up a typical lowered shape end to end") {
  // A temporary that is stored, loaded once into another temporary, and never
  // used again, plus a constant expression.
  primec::IrModule module = moduleOf(assemble({
      "PushI32 4",
      "PushI32 6",
      "AddI32",
      "StoreLocal 0",
      "LoadLocal 0",
      "StoreLocal 1",
      "LoadLocal 0",
      "ReturnI32",
  }));
  primec::OptimizationOptions options;
  options.level = 1;
  options.verifyEachPass = true;
  primec::IrOptimizationReport report;
  std::string error;
  REQUIRE_MESSAGE(
      primec::optimizeIrModule(module, options, primec::IrValidationTarget::Any, report, error),
      error);
  // `StoreLocal 1` became a pop of the loaded value, which the next round removed.
  const primec::IrModule simplified = moduleOf(assemble({
      "PushI32 10",
      "StoreLocal 0",
      "LoadLocal 0",
      "ReturnI32",
  }));
  CHECK_MESSAGE(listing(module) == listing(simplified),
                "\nactual:\n",
                listing(module),
                "expected:\n",
                listing(simplified));
  CHECK(report.instructionsBefore == 8);
  CHECK(report.instructionsAfter == 4);
}

namespace {
const primec::IrPreparationPhaseManifestEntry *findPhase(std::string_view name) {
  for (const auto &entry : primec::irPreparationPhaseManifest()) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}
} // namespace

TEST_CASE("ir preparation manifest places optimization between inlining and AST release") {
  const auto *validateInlined = findPhase("validate-inlined-ir");
  const auto *optimize = findPhase("optimize-ir");
  const auto *validateOptimized = findPhase("validate-optimized-ir");
  const auto *release = findPhase("release-lowered-ast-bodies");
  REQUIRE(validateInlined != nullptr);
  REQUIRE(optimize != nullptr);
  REQUIRE(validateOptimized != nullptr);
  REQUIRE(release != nullptr);
  CHECK(validateInlined < optimize);
  CHECK(optimize < validateOptimized);
  CHECK(validateOptimized < release);

  CHECK(optimize->optional);
  CHECK(optimize->inputOwnership == primec::IrPreparationPhaseOwnership::IrPreparationValidatedIr);
  CHECK(optimize->outputOwnership == primec::IrPreparationPhaseOwnership::IrPreparationOptimizedIr);
  CHECK(optimize->action == primec::IrPreparationPhaseAction::MutatesOutput);
  CHECK(std::string_view(optimize->invalidationNotes)
            .find("invalidates the prior validation result") != std::string_view::npos);
  CHECK(validateOptimized->optional);
  CHECK(validateOptimized->inputOwnership ==
        primec::IrPreparationPhaseOwnership::IrPreparationOptimizedIr);
  CHECK(validateOptimized->outputOwnership ==
        primec::IrPreparationPhaseOwnership::IrPreparationValidatedIr);
  CHECK(validateOptimized->action == primec::IrPreparationPhaseAction::ValidatesOnly);
}

TEST_CASE("pass selection follows level, explicit enables, and disables that win") {
  const auto &registry = primec::irOptimizationPasses();
  const auto select = [&](primec::OptimizationOptions options, primec::IrValidationTarget target) {
    std::vector<size_t> selected;
    std::string error;
    REQUIRE_MESSAGE(primec::selectIrOptimizationPasses(registry, options, target, selected, error),
                    error);
    std::vector<std::string> names;
    for (const size_t index : selected) {
      names.emplace_back(registry[index].info.name);
    }
    return names;
  };
  using Names = std::vector<std::string>;
  primec::OptimizationOptions options;
  CHECK(select(options, primec::IrValidationTarget::Vm).empty());
  options.level = 1;
  CHECK(select(options, primec::IrValidationTarget::Vm) ==
        Names{"cfg-simplify", "const-fold", "peephole", "dead-store"});
  // Structured targets skip control-flow rewriting silently at a level...
  CHECK(select(options, primec::IrValidationTarget::Wasm) ==
        Names{"const-fold", "peephole", "dead-store"});
  // ...and GPU targets run nothing.
  CHECK(select(options, primec::IrValidationTarget::Glsl).empty());

  options = {};
  options.enabledPasses = {"dead-store", "peephole"};
  CHECK(select(options, primec::IrValidationTarget::Native) ==
        Names{"peephole", "dead-store"}); // registry order
  options.level = 1;
  options.disabledPasses = {"peephole"};
  CHECK(select(options, primec::IrValidationTarget::Native) ==
        Names{"cfg-simplify", "const-fold", "dead-store"});
  options.enabledPasses = {"peephole"}; // a disable wins over an enable
  CHECK(select(options, primec::IrValidationTarget::Native) ==
        Names{"cfg-simplify", "const-fold", "dead-store"});
}

namespace {
bool markNothingChanged(primec::IrModule &,
                        const primec::IrPassContext &,
                        bool &changed,
                        std::string &) {
  changed = false;
  return true;
}
// Appends a Pop/Push pair, breaking nothing but changing the module; used to
// prove verification and ordering behave as documented.
bool appendNoOpPair(primec::IrModule &module,
                    const primec::IrPassContext &,
                    bool &changed,
                    std::string &) {
  auto &code = module.functions[0].instructions;
  code.insert(code.begin(), {primec::IrOpcode::PushI32, 1, 0});
  code.insert(code.begin() + 1, {primec::IrOpcode::Pop, 0, 0});
  changed = true;
  return true;
}
// Leaves a value on the stack at a return: valid opcodes, inconsistent depth at the join.
bool breakStackBalance(primec::IrModule &module,
                       const primec::IrPassContext &,
                       bool &changed,
                       std::string &) {
  module.functions[0].instructions.insert(module.functions[0].instructions.begin(),
                                          {primec::IrOpcode::Pop, 0, 0});
  changed = true;
  return true;
}
bool failWithError(primec::IrModule &, const primec::IrPassContext &, bool &, std::string &error) {
  error = "boom";
  return false;
}
} // namespace

TEST_CASE("the manager runs fake passes in registry order, repeats while something changes, and "
          "verifies each change") {
  const std::vector<primec::IrOptimizationPass> registry = {
      {{"first", "test", 1, primec::IrTargetsAll}, &markNothingChanged},
      {{"second", "test", 1, primec::IrTargetsAll}, &appendNoOpPair},
  };
  primec::OptimizationOptions options;
  options.level = 1;
  options.verifyEachPass = true;
  primec::IrModule module = moduleOf(assemble({"ReturnVoid"}));
  primec::IrOptimizationReport report;
  std::string error;
  REQUIRE_MESSAGE(primec::optimizeIrModuleWithPasses(
                      module, registry, options, primec::IrValidationTarget::Any, report, error),
                  error);
  // `second` changes the module every round, so the manager stops at its round bound.
  REQUIRE(report.runs.size() == 8);
  CHECK(report.runs[0].name == "first");
  CHECK(report.runs[1].name == "second");
  CHECK(report.runs[0].round == 1);
  CHECK(report.runs[2].round == 2);
  CHECK(report.runs[3].changed);
  CHECK(report.instructionsBefore == 1);
  CHECK(report.instructionsAfter == 9);
  CHECK(report.selectedPasses == std::vector<std::string>{"first", "second"});
  const std::string text = report.format(false);
  CHECK(text.find("time_us") == std::string::npos);
  CHECK(report.format(true).find("time_us=") != std::string::npos);
}

TEST_CASE("verify-each catches a pass that breaks the operand stack, and failures are reported") {
  primec::OptimizationOptions options;
  options.level = 1;
  options.verifyEachPass = true;
  std::string error;
  primec::IrOptimizationReport report;

  primec::IrModule module = moduleOf(assemble({"ReturnVoid"}));
  const std::vector<primec::IrOptimizationPass> breaking = {
      {{"breaker", "test", 1, primec::IrTargetsAll}, &breakStackBalance}};
  CHECK_FALSE(primec::optimizeIrModuleWithPasses(
      module, breaking, options, primec::IrValidationTarget::Any, report, error));
  CHECK(error.find("optimization pass breaker produced invalid IR") != std::string::npos);
  CHECK(error.find("operand stack") != std::string::npos);

  // Without verify-each the manager does not look; prepareIrModule's final validation does.
  options.verifyEachPass = false;
  module = moduleOf(assemble({"ReturnVoid"}));
  CHECK(primec::optimizeIrModuleWithPasses(
      module, breaking, options, primec::IrValidationTarget::Any, report, error));

  const std::vector<primec::IrOptimizationPass> failing = {
      {{"failer", "test", 1, primec::IrTargetsAll}, &failWithError}};
  module = moduleOf(assemble({"ReturnVoid"}));
  CHECK_FALSE(primec::optimizeIrModuleWithPasses(
      module, failing, options, primec::IrValidationTarget::Any, report, error));
  CHECK(error == "optimization pass failer failed: boom");
}

TEST_CASE("copy-prop reads the original local instead of its copy") {
  expectGolden("copy-prop",
               {"a load of the copy reads the source",
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "LoadLocal 1",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"}),
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "LoadLocal 0",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"})});
  expectGolden("copy-prop",
               {"chains collapse to the root",
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "LoadLocal 1",
                          "StoreLocal 2",
                          "LoadLocal 2",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"}),
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "LoadLocal 0",
                          "StoreLocal 2",
                          "LoadLocal 0",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"})});
}

TEST_CASE("copy-prop stops at writes to either side of the copy") {
  const Golden unchanged[] = {
      {"source overwritten after the copy",
       assemble({"PushI32 7",
                 "StoreLocal 0",
                 "LoadLocal 0",
                 "StoreLocal 1",
                 "PushI32 9",
                 "StoreLocal 0",
                 "LoadLocal 1",
                 "PrintI32 1",
                 "PushI32 0",
                 "ReturnI32"}),
       assemble({"PushI32 7",
                 "StoreLocal 0",
                 "LoadLocal 0",
                 "StoreLocal 1",
                 "PushI32 9",
                 "StoreLocal 0",
                 "LoadLocal 1",
                 "PrintI32 1",
                 "PushI32 0",
                 "ReturnI32"})},
      {"copy overwritten after the copy",
       assemble({"PushI32 7",
                 "StoreLocal 0",
                 "LoadLocal 0",
                 "StoreLocal 1",
                 "PushI32 9",
                 "StoreLocal 1",
                 "LoadLocal 1",
                 "PrintI32 1",
                 "PushI32 0",
                 "ReturnI32"}),
       assemble({"PushI32 7",
                 "StoreLocal 0",
                 "LoadLocal 0",
                 "StoreLocal 1",
                 "PushI32 9",
                 "StoreLocal 1",
                 "LoadLocal 1",
                 "PrintI32 1",
                 "PushI32 0",
                 "ReturnI32"})},
  };
  for (const Golden &golden : unchanged) {
    expectGolden("copy-prop", golden);
  }
}

TEST_CASE("copy-prop respects joins and loops") {
  // Both arms make the same copy, so it holds after the join.
  expectGolden("copy-prop",
               {"the same copy on both paths survives the join",
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 2",
                          "JumpIfZero 7",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "Jump 9",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "LoadLocal 1",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"}),
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 2",
                          "JumpIfZero 7",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "Jump 9",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "LoadLocal 0",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"})});
  // Only one arm copies, so the join knows nothing.
  expectGolden("copy-prop",
               {"a copy made on one path only does not survive the join",
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 2",
                          "JumpIfZero 7",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "Jump 9",
                          "PushI32 3",
                          "StoreLocal 1",
                          "LoadLocal 1",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"}),
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 2",
                          "JumpIfZero 7",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "Jump 9",
                          "PushI32 3",
                          "StoreLocal 1",
                          "LoadLocal 1",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"})});
  // The source changes inside the loop body, so the copy made before the loop
  // is not valid at the loop head on later iterations.
  expectGolden("copy-prop",
               {"a copy invalidated on the back edge is not propagated into the loop",
                assemble({"PushI32 3",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "LoadLocal 1",
                          "PrintI32 1",
                          "LoadLocal 0",
                          "PushI32 1",
                          "SubI32",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "JumpIfZero 13",
                          "Jump 4",
                          "PushI32 0",
                          "ReturnI32"}),
                assemble({"PushI32 3",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "LoadLocal 1",
                          "PrintI32 1",
                          "LoadLocal 0",
                          "PushI32 1",
                          "SubI32",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "JumpIfZero 13",
                          "Jump 4",
                          "PushI32 0",
                          "ReturnI32"})});
}

TEST_CASE("copy-prop leaves functions that take local addresses alone") {
  expectGolden("copy-prop",
               {"address taken",
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "AddressOfLocal 0",
                          "PushI32 9",
                          "StoreIndirect",
                          "Pop",
                          "LoadLocal 1",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"}),
                assemble({"PushI32 7",
                          "StoreLocal 0",
                          "LoadLocal 0",
                          "StoreLocal 1",
                          "AddressOfLocal 0",
                          "PushI32 9",
                          "StoreIndirect",
                          "Pop",
                          "LoadLocal 1",
                          "PrintI32 1",
                          "PushI32 0",
                          "ReturnI32"})});
}

TEST_CASE("peephole drops the not-equal-zero test of a comparison result") {
  expectGolden("peephole",
               {"a comparison is already a boolean",
                assemble({"LoadLocal 0",
                          "LoadLocal 1",
                          "CmpLtI32",
                          "PushI32 0",
                          "CmpNeI32",
                          "JumpIfZero 8",
                          "PushI32 1",
                          "ReturnI32",
                          "PushI32 2",
                          "ReturnI32"}),
                assemble({"LoadLocal 0",
                          "LoadLocal 1",
                          "CmpLtI32",
                          "JumpIfZero 6",
                          "PushI32 1",
                          "ReturnI32",
                          "PushI32 2",
                          "ReturnI32"})});
  expectGolden("peephole",
               {"a value that is not a comparison keeps its test",
                assemble({"LoadLocal 0",
                          "PushI32 0",
                          "CmpNeI32",
                          "JumpIfZero 6",
                          "PushI32 1",
                          "ReturnI32",
                          "PushI32 2",
                          "ReturnI32"}),
                assemble({"LoadLocal 0",
                          "PushI32 0",
                          "CmpNeI32",
                          "JumpIfZero 6",
                          "PushI32 1",
                          "ReturnI32",
                          "PushI32 2",
                          "ReturnI32"})});
  // A path that jumps to the pushed zero skips the comparison, so the test stays.
  expectGolden("peephole",
               {"a join point at the pushed zero keeps the test",
                assemble({"LoadLocal 0",
                          "JumpIfZero 6",
                          "LoadLocal 1",
                          "LoadLocal 2",
                          "CmpLtI32",
                          "Jump 7",
                          "PushI32 5",
                          "PushI32 0",
                          "CmpNeI32",
                          "JumpIfZero 12",
                          "PushI32 1",
                          "ReturnI32",
                          "PushI32 2",
                          "ReturnI32"}),
                assemble({"LoadLocal 0",
                          "JumpIfZero 6",
                          "LoadLocal 1",
                          "LoadLocal 2",
                          "CmpLtI32",
                          "Jump 7",
                          "PushI32 5",
                          "PushI32 0",
                          "CmpNeI32",
                          "JumpIfZero 12",
                          "PushI32 1",
                          "ReturnI32",
                          "PushI32 2",
                          "ReturnI32"})});
}

TEST_CASE("validateIrModule rejects unbalanced operand stacks for every target") {
  struct Bad {
    const char *name;
    std::vector<primec::IrInstruction> body;
    const char *message;
  };
  const std::vector<Bad> cases = {
      {"pop on an empty stack", assemble({"Pop", "ReturnVoid"}), "operand stack underflow"},
      {"binary operator missing an operand",
       assemble({"PushI32 1", "AddI32", "ReturnI32"}),
       "operand stack underflow"},
      {"dup on an empty stack", assemble({"Dup", "ReturnI32"}), "dup with an empty operand stack"},
      {"join with different depths",
       assemble({"LoadLocal 0", "JumpIfZero 4", "PushI32 1", "PushI32 2", "ReturnI32"}),
       "different operand stack depths"},
  };
  const primec::IrValidationTarget targets[] = {primec::IrValidationTarget::Any,
                                                primec::IrValidationTarget::Vm,
                                                primec::IrValidationTarget::Native,
                                                primec::IrValidationTarget::Glsl};
  for (const Bad &bad : cases) {
    for (const primec::IrValidationTarget target : targets) {
      CAPTURE(bad.name);
      const primec::IrModule module = moduleOf(bad.body);
      std::string error;
      CHECK_FALSE(primec::validateIrModule(module, target, error));
      CHECK_MESSAGE(error.find(bad.message) != std::string::npos, error);
    }
  }

  // A balanced module with a join of equal depths still passes.
  const primec::IrModule fine = moduleOf(
      assemble({"LoadLocal 0", "JumpIfZero 4", "PushI32 1", "Jump 5", "PushI32 2", "ReturnI32"}));
  std::string error;
  CHECK_MESSAGE(primec::validateIrModule(fine, primec::IrValidationTarget::Vm, error), error);
}
