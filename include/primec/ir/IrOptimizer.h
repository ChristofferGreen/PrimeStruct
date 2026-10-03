#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/ir/IrValidation.h"
#include "primec/support/Options.h"

namespace primec {

// Target-independent IR optimizer (docs/OptimizingBackendsPlan.md). Passes
// rewrite an IrModule in place; every pass must leave a module that validates
// for the same target and behaves identically: same output, exit code and
// faults. The pipeline runs the selected passes in manifest order and repeats
// the whole sequence until nothing changes (bounded), so passes may expose
// work for each other.

// Bit per IrValidationTarget, for IrOptimizationPassInfo::targets.
constexpr uint32_t irValidationTargetBit(IrValidationTarget target) {
  return 1u << static_cast<uint32_t>(target);
}

constexpr uint32_t IrTargetsAll = irValidationTargetBit(IrValidationTarget::Any) |
                                  irValidationTargetBit(IrValidationTarget::Serialized) |
                                  irValidationTargetBit(IrValidationTarget::Vm) |
                                  irValidationTargetBit(IrValidationTarget::Native) |
                                  irValidationTargetBit(IrValidationTarget::Glsl) |
                                  irValidationTargetBit(IrValidationTarget::Wasm) |
                                  irValidationTargetBit(IrValidationTarget::WasmBrowser);
// Everything except the structured-control-flow targets (wasm, glsl).
constexpr uint32_t IrTargetsUnstructured = irValidationTargetBit(IrValidationTarget::Any) |
                                           irValidationTargetBit(IrValidationTarget::Serialized) |
                                           irValidationTargetBit(IrValidationTarget::Vm) |
                                           irValidationTargetBit(IrValidationTarget::Native);
// The unstructured targets plus wasm; GLSL/SPIR-V are excluded because their
// emitters reconstruct shader control flow and literal limits from the IR.
constexpr uint32_t IrTargetsNoGpu = IrTargetsUnstructured |
                                    irValidationTargetBit(IrValidationTarget::Wasm) |
                                    irValidationTargetBit(IrValidationTarget::WasmBrowser);

struct IrOptimizationPassInfo {
  std::string_view name;
  std::string_view description;
  // Lowest -O level at which the pass runs by default (1..3); 0 means it runs
  // only when named by --opt-pass.
  uint8_t defaultFromLevel = 0;
  // IrValidationTarget bits the pass may run for.
  uint32_t targets = IrTargetsAll;
};

struct IrPassContext {
  IrValidationTarget target = IrValidationTarget::Any;
};

struct IrOptimizationPass {
  IrOptimizationPassInfo info;
  // Rewrites the module. Sets `changed` when it modified anything; returns
  // false with `error` set only for an internal failure.
  bool (*run)(IrModule &module,
              const IrPassContext &context,
              bool &changed,
              std::string &error) = nullptr;
};

struct IrOptimizationPassReport {
  std::string name;
  uint32_t round = 0;
  uint64_t instructionsBefore = 0;
  uint64_t instructionsAfter = 0;
  bool changed = false;
  uint64_t microseconds = 0;
};

struct IrOptimizationReport {
  uint8_t level = 0;
  std::vector<std::string> selectedPasses;
  std::vector<IrOptimizationPassReport> runs;
  uint64_t instructionsBefore = 0;
  uint64_t instructionsAfter = 0;

  // Deterministic text for --opt-report; timings are included only on request
  // because they vary between runs.
  std::string format(bool includeTimings) const;
};

// Built-in passes, in the order they run.
const std::vector<IrOptimizationPass> &irOptimizationPasses();

// Text for --opt-list: one line per built-in pass (name, default level, targets,
// description), in manifest order.
std::string formatIrOptimizationPassList();

// Resolves which passes of `registry` run: those whose defaultFromLevel is at
// or below the level, plus --opt-pass names, minus --no-opt-pass names (a
// disable wins over an enable). Fails on an unknown name or on an explicitly
// enabled pass that does not support `target`; passes enabled only by the
// level are skipped silently for unsupported targets. `selected` holds
// registry indices in registry order.
bool selectIrOptimizationPasses(const std::vector<IrOptimizationPass> &registry,
                                const OptimizationOptions &options,
                                IrValidationTarget target,
                                std::vector<size_t> &selected,
                                std::string &error);

// Runs the selected built-in passes. `report` is filled even on failure.
bool optimizeIrModule(IrModule &module,
                      const OptimizationOptions &options,
                      IrValidationTarget target,
                      IrOptimizationReport &report,
                      std::string &error);

// Same with an explicit registry; the seam tests use to run fake passes.
bool optimizeIrModuleWithPasses(IrModule &module,
                                const std::vector<IrOptimizationPass> &registry,
                                const OptimizationOptions &options,
                                IrValidationTarget target,
                                IrOptimizationReport &report,
                                std::string &error);

// True when no pass would run for these options and target (so a caller can
// skip the phase, and its re-validation, entirely).
bool irOptimizationIsNoOp(const OptimizationOptions &options, IrValidationTarget target);

// Checks the structural invariants every pass must preserve: the module
// validates for `target` and every function's reachable code has a consistent
// operand-stack depth. Used by --opt-verify-each after each pass.
bool verifyIrModuleForOptimization(const IrModule &module,
                                   IrValidationTarget target,
                                   std::string &error);

} // namespace primec
