#include "primec/ir/IrOptimizer.h"

#include "primec/support/CompileArena.h"

#include "optimizer/IrPassUtil.h"
#include "primec/ir/IrCfg.h"
#include "primec/ir/IrOpcodeTable.h"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace primec {
namespace {

// Rounds of the whole selected sequence. Each round only runs while the
// previous one changed something, so well-behaved input stops after two.
constexpr uint32_t MaxRounds = 4;

uint64_t countInstructions(const IrModule &module) {
  uint64_t total = 0;
  for (const IrFunction &function : module.functions) {
    total += function.instructions.size();
  }
  return total;
}

std::string targetName(IrValidationTarget target) {
  switch (target) {
  case IrValidationTarget::Any:
    return "any";
  case IrValidationTarget::Serialized:
    return "serialized";
  case IrValidationTarget::Vm:
    return "vm";
  case IrValidationTarget::Native:
    return "native";
  case IrValidationTarget::Glsl:
    return "glsl";
  case IrValidationTarget::Wasm:
    return "wasm";
  case IrValidationTarget::WasmBrowser:
    return "wasm-browser";
  }
  return "?";
}

bool passSupportsTarget(const IrOptimizationPass &pass, IrValidationTarget target) {
  return (pass.info.targets & irValidationTargetBit(target)) != 0;
}

} // namespace

const std::vector<IrOptimizationPass> &irOptimizationPasses() {
  // Manifest order is execution order. Cheap structural cleanups come first so
  // later passes see straight-line code; copy-prop runs before dead-store so
  // the copies it leaves unread are dropped, and dead-store's leftover pops are
  // swept by the next round's peephole.
  // The table is built on first use, which can happen while a compile arena is
  // active; its storage must come from the system heap because it is destroyed
  // at process exit (see CompileArena.h).
  const SystemHeapScope systemHeapGuardForPassTable;
  static const std::vector<IrOptimizationPass> Passes = {
      {{"cfg-simplify",
        "fold constant branches, thread jumps, drop jumps to the next instruction and unreachable "
        "code",
        1,
        IrTargetsUnstructured},
       &ir_opt::runCfgSimplifyPass},
      {{"const-fold",
        "evaluate pure arithmetic, comparisons and conversions of constants",
        1,
        IrTargetsNoGpu},
       &ir_opt::runConstFoldPass},
      {{"peephole",
        "remove dead push/pop pairs, x+0, x*1, x/1 and double negation",
        1,
        IrTargetsNoGpu},
       &ir_opt::runPeepholePass},
      {{"copy-prop",
        "read the original local instead of a copy of it, so the copy can die",
        2,
        IrTargetsNoGpu},
       &ir_opt::runCopyPropPass},
      {{"dead-store", "turn stores to locals that are never read into pops", 1, IrTargetsNoGpu},
       &ir_opt::runDeadStorePass},
      {{"if-convert",
        "turn a local stepped by a constant under a comparison into straight-line code that adds "
        "the comparison times the step (native only, where branches cost more than the multiply)",
        2,
        irValidationTargetBit(IrValidationTarget::Native)},
       &ir_opt::runIfConvertPass},
      {{"loop-rotate",
        "move the test of a counting loop to its back edge, removing one jump per iteration (VM "
        "only: the native emitter's jump padding makes it slower there)",
        2,
        irValidationTargetBit(IrValidationTarget::Vm)},
       &ir_opt::runLoopRotatePass},
  };
  return Passes;
}

std::string formatIrOptimizationPassList() {
  std::ostringstream out;
  out << "name\tdefault_level\ttargets\tdescription\n";
  for (const IrOptimizationPass &pass : irOptimizationPasses()) {
    out << pass.info.name << '\t';
    if (pass.info.defaultFromLevel == 0) {
      out << "-";
    } else {
      out << "-O" << static_cast<int>(pass.info.defaultFromLevel);
    }
    out << '\t';
    bool first = true;
    for (const IrValidationTarget target : {IrValidationTarget::Any,
                                            IrValidationTarget::Serialized,
                                            IrValidationTarget::Vm,
                                            IrValidationTarget::Native,
                                            IrValidationTarget::Wasm,
                                            IrValidationTarget::WasmBrowser,
                                            IrValidationTarget::Glsl}) {
      if (passSupportsTarget(pass, target)) {
        out << (first ? "" : ",") << targetName(target);
        first = false;
      }
    }
    out << '\t' << pass.info.description << '\n';
  }
  return out.str();
}

bool selectIrOptimizationPasses(const std::vector<IrOptimizationPass> &registry,
                                const OptimizationOptions &options,
                                IrValidationTarget target,
                                std::vector<size_t> &selected,
                                std::string &error) {
  selected.clear();
  const auto findPass = [&](const std::string &name) -> int {
    for (size_t i = 0; i < registry.size(); ++i) {
      if (registry[i].info.name == name) {
        return static_cast<int>(i);
      }
    }
    return -1;
  };

  std::vector<bool> enabled(registry.size(), false);
  for (size_t i = 0; i < registry.size(); ++i) {
    const uint8_t from = registry[i].info.defaultFromLevel;
    enabled[i] = from != 0 && options.level >= from && passSupportsTarget(registry[i], target);
  }
  for (const std::string &name : options.enabledPasses) {
    const int index = findPass(name);
    if (index < 0) {
      error = "unknown optimization pass: " + name + " (see --opt-list)";
      return false;
    }
    if (!passSupportsTarget(registry[static_cast<size_t>(index)], target)) {
      error =
          "optimization pass " + name + " does not support the " + targetName(target) + " target";
      return false;
    }
    enabled[static_cast<size_t>(index)] = true;
  }
  for (const std::string &name : options.disabledPasses) {
    const int index = findPass(name);
    if (index < 0) {
      error = "unknown optimization pass: " + name + " (see --opt-list)";
      return false;
    }
    enabled[static_cast<size_t>(index)] = false;
  }
  for (size_t i = 0; i < registry.size(); ++i) {
    if (enabled[i]) {
      selected.push_back(i);
    }
  }
  return true;
}

bool verifyIrModuleForOptimization(const IrModule &module,
                                   IrValidationTarget target,
                                   std::string &error) {
  if (!validateIrModule(module, target, error)) {
    return false;
  }
  for (const IrFunction &function : module.functions) {
    IrCfg cfg;
    IrCfgError cfgError;
    if (!buildIrCfg(function, module, cfg, cfgError)) {
      const IrOpcodeInfo *info = irOpcodeInfo(cfgError.opcode);
      error = "inconsistent operand stack in " + function.name + " at instruction " +
              std::to_string(cfgError.instructionIndex) + " (" +
              (info != nullptr ? info->name : "?") + ")";
      return false;
    }
  }
  return true;
}

bool irOptimizationIsNoOp(const OptimizationOptions &options, IrValidationTarget target) {
  std::vector<size_t> selected;
  std::string error;
  if (!selectIrOptimizationPasses(irOptimizationPasses(), options, target, selected, error)) {
    return false; // let optimizeIrModule report the error
  }
  return selected.empty();
}

std::string IrOptimizationReport::format(bool includeTimings) const {
  std::ostringstream out;
  out << "optimization_report_v1\n";
  out << "level=" << static_cast<int>(level) << "\n";
  out << "selected_passes=";
  for (size_t i = 0; i < selectedPasses.size(); ++i) {
    out << (i == 0 ? "" : ",") << selectedPasses[i];
  }
  out << "\n";
  out << "instructions_before=" << instructionsBefore << "\n";
  out << "instructions_after=" << instructionsAfter << "\n";
  for (const IrOptimizationPassReport &run : runs) {
    out << "round " << run.round << " " << run.name << ": " << run.instructionsBefore << " -> "
        << run.instructionsAfter << (run.changed ? " changed" : " unchanged");
    if (includeTimings) {
      out << " time_us=" << run.microseconds;
    }
    out << "\n";
  }
  return out.str();
}

bool optimizeIrModuleWithPasses(IrModule &module,
                                const std::vector<IrOptimizationPass> &registry,
                                const OptimizationOptions &options,
                                IrValidationTarget target,
                                IrOptimizationReport &report,
                                std::string &error) {
  report = {};
  report.level = options.level;
  report.instructionsBefore = countInstructions(module);
  report.instructionsAfter = report.instructionsBefore;

  std::vector<size_t> selected;
  if (!selectIrOptimizationPasses(registry, options, target, selected, error)) {
    return false;
  }
  for (const size_t index : selected) {
    report.selectedPasses.emplace_back(registry[index].info.name);
  }

  const IrPassContext context{target};
  for (uint32_t round = 1; round <= MaxRounds && !selected.empty(); ++round) {
    bool roundChanged = false;
    for (const size_t index : selected) {
      const IrOptimizationPass &pass = registry[index];
      IrOptimizationPassReport run;
      run.name = std::string(pass.info.name);
      run.round = round;
      run.instructionsBefore = countInstructions(module);
      const auto started = std::chrono::steady_clock::now();
      bool changed = false;
      std::string passError;
      const bool ok = pass.run(module, context, changed, passError);
      run.microseconds =
          static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now() - started)
                                    .count());
      run.instructionsAfter = countInstructions(module);
      run.changed = changed;
      report.runs.push_back(run);
      report.instructionsAfter = run.instructionsAfter;
      if (!ok) {
        error = "optimization pass " + run.name + " failed: " + passError;
        return false;
      }
      if (changed) {
        roundChanged = true;
        if (options.verifyEachPass) {
          std::string verifyError;
          if (!verifyIrModuleForOptimization(module, target, verifyError)) {
            error = "optimization pass " + run.name + " produced invalid IR: " + verifyError;
            return false;
          }
        }
      }
    }
    if (!roundChanged) {
      break;
    }
  }
  return true;
}

bool optimizeIrModule(IrModule &module,
                      const OptimizationOptions &options,
                      IrValidationTarget target,
                      IrOptimizationReport &report,
                      std::string &error) {
  return optimizeIrModuleWithPasses(module, irOptimizationPasses(), options, target, report, error);
}

} // namespace primec
