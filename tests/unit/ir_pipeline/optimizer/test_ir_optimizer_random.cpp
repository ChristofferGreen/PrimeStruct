#include "primec/ir/IrModulePrinter.h"
#include "primec/ir/IrOptimizer.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_random_programs.h"
#include "test_ir_vm_run.h"

#include <cstdint>
#include <string>

TEST_SUITE_BEGIN("primestruct.ir.optimizer_random");

// Seeded programs of valid, balanced stack IR (assignments, the dup/store/pop
// assignment idiom, discarded expressions, if/else, counted loops, integer and
// float expressions) are run through the VM before and after optimization; any
// difference in output, exit code or fault is a bug in a pass. The generator
// avoids operations that fault (division uses a nonzero constant) so every
// program terminates normally.

namespace {

using optimizer_test::Outcome;
using optimizer_test::makeModule;
using optimizer_test::run;

// Optimizes a copy with `options` and returns whether it matched the original.
bool optimizedMatches(const primec::IrModule &original,
                      const primec::OptimizationOptions &options,
                      const Outcome &baseline,
                      std::string &detail) {
  primec::IrModule optimized = original;
  primec::IrOptimizationReport report;
  std::string error;
  if (!primec::optimizeIrModule(optimized, options, primec::IrValidationTarget::Vm, report, error)) {
    detail = "optimizer failed: " + error;
    return false;
  }
  const Outcome after = run(optimized);
  if (!(after == baseline)) {
    detail = "behavior differs; original:\n" + primec::formatIrFunction(original, 0) + "optimized:\n" +
             primec::formatIrFunction(optimized, 0);
    return false;
  }
  return true;
}

} // namespace

TEST_CASE("generated programs are valid and exercise loops and branches") {
  size_t withLoop = 0;
  size_t withBranch = 0;
  for (uint64_t seed = 0; seed < 200; ++seed) {
    size_t loops = 0;
    size_t branches = 0;
    const primec::IrModule module = makeModule(seed, &loops, &branches);
    std::string error;
    REQUIRE_MESSAGE(primec::verifyIrModuleForOptimization(module, primec::IrValidationTarget::Vm, error),
                    "seed ", seed, ": ", error);
    withLoop += loops > 0 ? 1 : 0;
    withBranch += branches > 0 ? 1 : 0;
    const Outcome outcome = run(module);
    CHECK_MESSAGE(outcome.ok, "seed ", seed, " faulted: ", outcome.error);
  }
  CHECK(withLoop > 40);
  CHECK(withBranch > 40);
}

TEST_CASE("optimizing random programs at O1 preserves their behavior") {
  primec::OptimizationOptions options;
  options.level = 1;
  options.verifyEachPass = true;
  size_t shrunk = 0;
  for (uint64_t seed = 1000; seed < 2200; ++seed) {
    const primec::IrModule original = makeModule(seed);
    const Outcome baseline = run(original);
    REQUIRE_MESSAGE(baseline.ok, "seed ", seed, ": ", baseline.error);
    std::string detail;
    CHECK_MESSAGE(optimizedMatches(original, options, baseline, detail), "seed ", seed, ": ", detail);

    primec::IrModule optimized = original;
    primec::IrOptimizationReport report;
    std::string error;
    REQUIRE(primec::optimizeIrModule(optimized, options, primec::IrValidationTarget::Vm, report, error));
    shrunk += report.instructionsAfter < report.instructionsBefore ? 1 : 0;
  }
  // The generator emits patterns the passes target; if almost nothing shrinks
  // the test no longer exercises them.
  CHECK(shrunk > 600);
}

TEST_CASE("optimizing random programs at O2 and O3 preserves their behavior") {
  for (const uint8_t level : {2, 3}) {
    CAPTURE(level);
    primec::OptimizationOptions options;
    options.level = level;
    options.verifyEachPass = true;
    for (uint64_t seed = 20000; seed < 20600; ++seed) {
      const primec::IrModule original = makeModule(seed);
      const Outcome baseline = run(original);
      REQUIRE_MESSAGE(baseline.ok, "seed ", seed, ": ", baseline.error);
      std::string detail;
      CHECK_MESSAGE(
          optimizedMatches(original, options, baseline, detail), "seed ", seed, ": ", detail);
    }
  }
}

TEST_CASE("each pass alone preserves the behavior of random programs") {
  for (const primec::IrOptimizationPass &pass : primec::irOptimizationPasses()) {
    CAPTURE(pass.info.name);
    primec::OptimizationOptions options;
    options.enabledPasses = {std::string(pass.info.name)};
    options.verifyEachPass = true;
    for (uint64_t seed = 5000; seed < 5400; ++seed) {
      const primec::IrModule original = makeModule(seed);
      const Outcome baseline = run(original);
      REQUIRE_MESSAGE(baseline.ok, "seed ", seed, ": ", baseline.error);
      std::string detail;
      CHECK_MESSAGE(optimizedMatches(original, options, baseline, detail), "seed ", seed, ": ", detail);
    }
  }
}

TEST_CASE("optimization is deterministic and idempotent") {
  primec::OptimizationOptions options;
  options.level = 1;
  for (uint64_t seed = 9000; seed < 9150; ++seed) {
    const primec::IrModule original = makeModule(seed);
    primec::IrModule first = original;
    primec::IrModule second = original;
    primec::IrOptimizationReport report;
    std::string error;
    REQUIRE(primec::optimizeIrModule(first, options, primec::IrValidationTarget::Vm, report, error));
    REQUIRE(primec::optimizeIrModule(second, options, primec::IrValidationTarget::Vm, report, error));
    CHECK_MESSAGE(primec::formatIrModule(first) == primec::formatIrModule(second), "seed ", seed);

    // Running the pipeline again on its own output changes nothing.
    primec::IrModule again = first;
    REQUIRE(primec::optimizeIrModule(again, options, primec::IrValidationTarget::Vm, report, error));
    CHECK_MESSAGE(primec::formatIrModule(again) == primec::formatIrModule(first), "seed ", seed, " is not idempotent");
  }
}
