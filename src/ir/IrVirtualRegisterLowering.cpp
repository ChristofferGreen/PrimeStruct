#include "primec/ir/IrVirtualRegisterLowering.h"

#include "primec/ir/IrCfg.h"
#include "primec/ir/IrLocalEscape.h"

#include <algorithm>
#include <map>

#include <string>
#include <utility>
#include <vector>

namespace primec {
namespace {

// Block structure, reachability and stack depths come from the shared CFG in
// IrCfg.h; this file only assigns virtual registers on top of it.
std::string formatCfgError(const IrCfgError &cfgError) {
  switch (cfgError.kind) {
  case IrCfgErrorKind::InvalidJumpTarget:
    return "virtual-register lowering found invalid jump target";
  case IrCfgErrorKind::UnsupportedOpcode:
    return "unsupported opcode in virtual-register lowering";
  case IrCfgErrorKind::StackUnderflow:
    return "virtual-register lowering found stack underflow at instruction " +
           std::to_string(cfgError.instructionIndex);
  case IrCfgErrorKind::InvalidDup:
    return "virtual-register lowering found invalid dup at instruction " +
           std::to_string(cfgError.instructionIndex);
  case IrCfgErrorKind::InconsistentDepth:
    return "virtual-register lowering found inconsistent stack depth at block boundary";
  case IrCfgErrorKind::None:
    break;
  }
  return "virtual-register lowering failed";
}

// Backward liveness of the promoted locals over the CFG: which of them hold a value that some
// path still reads at each block's entry and exit. Only LoadLocal reads and StoreLocal defines
// a promoted local (FileReadByte's slot is pinned, so never promoted).
struct LocalLiveness {
  std::vector<std::vector<uint32_t>> liveIn;
  std::vector<std::vector<uint32_t>> liveOut;
};

LocalLiveness computeLocalLiveness(const IrFunction &function,
                                   const IrCfg &cfg,
                                   const std::vector<bool> &promoted) {
  const size_t blocks = cfg.blocks.size();
  std::vector<std::vector<bool>> useBeforeDef(blocks, std::vector<bool>(promoted.size(), false));
  std::vector<std::vector<bool>> defined(blocks, std::vector<bool>(promoted.size(), false));
  for (size_t block = 0; block < blocks; ++block) {
    if (!cfg.blocks[block].reachable) {
      continue;
    }
    for (size_t index = cfg.blocks[block].start; index < cfg.blocks[block].end; ++index) {
      const IrInstruction &instruction = function.instructions[index];
      if ((instruction.op != IrOpcode::LoadLocal && instruction.op != IrOpcode::StoreLocal) ||
          instruction.imm >= promoted.size() || !promoted[static_cast<size_t>(instruction.imm)]) {
        continue;
      }
      const size_t local = static_cast<size_t>(instruction.imm);
      if (instruction.op == IrOpcode::LoadLocal) {
        if (!defined[block][local]) {
          useBeforeDef[block][local] = true;
        }
      } else {
        defined[block][local] = true;
      }
    }
  }
  std::vector<std::vector<bool>> liveIn(blocks, std::vector<bool>(promoted.size(), false));
  std::vector<std::vector<bool>> liveOut(blocks, std::vector<bool>(promoted.size(), false));
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t block = blocks; block > 0; --block) {
      const size_t b = block - 1;
      if (!cfg.blocks[b].reachable) {
        continue;
      }
      for (size_t local = 0; local < promoted.size(); ++local) {
        bool out = false;
        for (const size_t successor : cfg.blocks[b].successors) {
          out = out || (cfg.blocks[successor].reachable && liveIn[successor][local]);
        }
        const bool in = useBeforeDef[b][local] || (out && !defined[b][local]);
        if (out != liveOut[b][local] || in != liveIn[b][local]) {
          liveOut[b][local] = out;
          liveIn[b][local] = in;
          changed = true;
        }
      }
    }
  }
  LocalLiveness result;
  result.liveIn.resize(blocks);
  result.liveOut.resize(blocks);
  for (size_t block = 0; block < blocks; ++block) {
    for (size_t local = 0; local < promoted.size(); ++local) {
      if (liveIn[block][local]) {
        result.liveIn[block].push_back(static_cast<uint32_t>(local));
      }
      if (liveOut[block][local]) {
        result.liveOut[block].push_back(static_cast<uint32_t>(local));
      }
    }
  }
  return result;
}

bool lowerFunctionToVirtualRegisters(const IrFunction &function,
                                     const IrModule &module,
                                     bool promoteLocals,
                                     IrVirtualRegisterFunction &out,
                                     std::string &error) {
  error.clear();
  out = {};
  out.name = function.name;
  out.metadata = function.metadata;
  out.localDebugSlots = function.localDebugSlots;
  if (function.instructions.empty()) {
    return true;
  }

  IrCfg cfg;
  IrCfgError cfgError;
  if (!buildIrCfg(function, module, cfg, cfgError)) {
    error = formatCfgError(cfgError);
    return false;
  }

  out.blocks.resize(cfg.blocks.size());

  std::vector<bool> promoted;
  LocalLiveness localLiveness;
  if (promoteLocals) {
    const IrLocalEscapeInfo escape = analyzeIrLocalEscape(function);
    promoted.assign(escape.localCount, true);
    for (const uint32_t pinned : escape.pinnedSlots) {
      promoted[pinned] = false;
    }
    for (uint32_t local = 0; local < promoted.size(); ++local) {
      if (promoted[local]) {
        out.promotedLocals.push_back(local);
      }
    }
    localLiveness = computeLocalLiveness(function, cfg, promoted);
  }

  uint32_t nextRegisterId = 0;
  for (size_t blockIndex = 0; blockIndex < cfg.blocks.size(); ++blockIndex) {
    const IrCfgBlock &inBlock = cfg.blocks[blockIndex];
    IrVirtualRegisterBlock &outBlock = out.blocks[blockIndex];
    outBlock.startInstructionIndex = inBlock.start;
    outBlock.endInstructionIndex = inBlock.end;
    outBlock.reachable = inBlock.reachable;

    if (outBlock.reachable) {
      const size_t entrySize = static_cast<size_t>(inBlock.entryDepth);
      outBlock.entryRegisters.reserve(entrySize);
      for (size_t slot = 0; slot < entrySize; ++slot) {
        outBlock.entryRegisters.push_back(nextRegisterId++);
      }
      if (promoteLocals) {
        for (const uint32_t local : localLiveness.liveIn[blockIndex]) {
          outBlock.entryLocals.push_back({local, nextRegisterId++});
        }
      }
    }
  }

  for (size_t blockIndex = 0; blockIndex < cfg.blocks.size(); ++blockIndex) {
    const IrCfgBlock &inBlock = cfg.blocks[blockIndex];
    IrVirtualRegisterBlock &outBlock = out.blocks[blockIndex];
    std::vector<uint32_t> stack = outBlock.entryRegisters;
    std::map<uint32_t, uint32_t> currentLocal;
    for (const IrVirtualRegisterLocalValue &value : outBlock.entryLocals) {
      currentLocal[value.local] = value.reg;
    }

    outBlock.instructions.reserve(inBlock.end - inBlock.start);
    for (size_t instructionIndex = inBlock.start; instructionIndex < inBlock.end; ++instructionIndex) {
      IrVirtualRegisterInstruction loweredInstruction;
      loweredInstruction.instruction = function.instructions[instructionIndex];

      if (!outBlock.reachable) {
        outBlock.instructions.push_back(std::move(loweredInstruction));
        continue;
      }

      // The CFG already proved the depths consistent, so the effect exists and
      // the stack holds every operand it needs.
      IrStackEffect effect;
      if (!computeIrStackEffect(loweredInstruction.instruction, module, effect)) {
        error = "unsupported opcode in virtual-register lowering";
        return false;
      }

      for (uint32_t read = 0; read < effect.readsWithoutPop; ++read) {
        loweredInstruction.useRegisters.push_back(stack[stack.size() - static_cast<size_t>(read) - 1]);
      }

      std::vector<uint32_t> poppedRegisters;
      poppedRegisters.reserve(effect.pops);
      for (uint32_t pop = 0; pop < effect.pops; ++pop) {
        poppedRegisters.push_back(stack.back());
        stack.pop_back();
      }
      for (auto it = poppedRegisters.rbegin(); it != poppedRegisters.rend(); ++it) {
        loweredInstruction.useRegisters.push_back(*it);
      }

      loweredInstruction.defRegisters.reserve(effect.pushes);
      for (uint32_t push = 0; push < effect.pushes; ++push) {
        const uint32_t reg = nextRegisterId++;
        loweredInstruction.defRegisters.push_back(reg);
        stack.push_back(reg);
      }

      if (promoteLocals && loweredInstruction.instruction.imm < promoted.size() &&
          promoted[static_cast<size_t>(loweredInstruction.instruction.imm)]) {
        const uint32_t local = static_cast<uint32_t>(loweredInstruction.instruction.imm);
        if (loweredInstruction.instruction.op == IrOpcode::LoadLocal) {
          const auto current = currentLocal.find(local);
          if (current == currentLocal.end()) {
            error = "virtual-register lowering lost the value of local " + std::to_string(local);
            return false;
          }
          loweredInstruction.localUseRegister = current->second;
        } else if (loweredInstruction.instruction.op == IrOpcode::StoreLocal) {
          const uint32_t reg = nextRegisterId++;
          loweredInstruction.localDefRegister = reg;
          currentLocal[local] = reg;
        }
      }

      outBlock.instructions.push_back(std::move(loweredInstruction));
    }

    outBlock.exitRegisters = stack;
    if (!outBlock.reachable) {
      continue;
    }
    if (promoteLocals) {
      for (const uint32_t local : localLiveness.liveOut[blockIndex]) {
        const auto current = currentLocal.find(local);
        if (current == currentLocal.end()) {
          error = "virtual-register lowering lost the value of local " + std::to_string(local) +
                  " at a block exit";
          return false;
        }
        outBlock.exitLocals.push_back({local, current->second});
      }
    }

    outBlock.successorEdges.reserve(inBlock.successors.size());
    for (size_t successorIndex : inBlock.successors) {
      IrVirtualRegisterEdge edge;
      edge.successorBlockIndex = successorIndex;
      const IrVirtualRegisterBlock &successorBlock = out.blocks[successorIndex];
      if (!successorBlock.reachable) {
        outBlock.successorEdges.push_back(std::move(edge));
        continue;
      }
      if (outBlock.exitRegisters.size() != successorBlock.entryRegisters.size()) {
        error = "virtual-register lowering found inconsistent stack width at block boundary";
        return false;
      }
      edge.stackMoves.reserve(outBlock.exitRegisters.size());
      for (size_t slot = 0; slot < outBlock.exitRegisters.size(); ++slot) {
        edge.stackMoves.push_back({outBlock.exitRegisters[slot], successorBlock.entryRegisters[slot]});
      }
      for (const IrVirtualRegisterLocalValue &entry : successorBlock.entryLocals) {
        const auto source = std::find_if(
            outBlock.exitLocals.begin(),
            outBlock.exitLocals.end(),
            [&](const IrVirtualRegisterLocalValue &exit) { return exit.local == entry.local; });
        if (source == outBlock.exitLocals.end()) {
          error = "virtual-register lowering found a local live into a block but not out of its "
                  "predecessor";
          return false;
        }
        edge.localMoves.push_back({entry.local, source->reg, entry.reg});
      }
      outBlock.successorEdges.push_back(std::move(edge));
    }
  }

  out.nextVirtualRegister = nextRegisterId;
  return true;
}

} // namespace

bool lowerIrModuleToBlockVirtualRegisters(const IrModule &module,
                                          IrVirtualRegisterModule &out,
                                          std::string &error,
                                          const IrVirtualRegisterLoweringOptions &options) {
  error.clear();
  out = {};
  out.entryIndex = module.entryIndex;
  out.stringTable = module.stringTable;
  out.structLayouts = module.structLayouts;
  out.instructionSourceMap = module.instructionSourceMap;
  out.functions.resize(module.functions.size());

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    if (!lowerFunctionToVirtualRegisters(module.functions[functionIndex],
                                         module,
                                         options.promoteLocals,
                                         out.functions[functionIndex],
                                         error)) {
      if (!error.empty()) {
        error = "virtual-register lowering failed in function " + module.functions[functionIndex].name + ": " + error;
      }
      return false;
    }
  }
  return true;
}

bool liftBlockVirtualRegistersToIrModule(const IrVirtualRegisterModule &virtualModule, IrModule &out, std::string &error) {
  error.clear();
  out = {};
  out.entryIndex = virtualModule.entryIndex;
  out.stringTable = virtualModule.stringTable;
  out.structLayouts = virtualModule.structLayouts;
  out.instructionSourceMap = virtualModule.instructionSourceMap;
  out.functions.reserve(virtualModule.functions.size());

  for (const IrVirtualRegisterFunction &virtualFunction : virtualModule.functions) {
    IrFunction loweredFunction;
    loweredFunction.name = virtualFunction.name;
    loweredFunction.metadata = virtualFunction.metadata;
    loweredFunction.localDebugSlots = virtualFunction.localDebugSlots;
    for (const IrVirtualRegisterBlock &block : virtualFunction.blocks) {
      for (const IrVirtualRegisterInstruction &instruction : block.instructions) {
        loweredFunction.instructions.push_back(instruction.instruction);
      }
    }
    out.functions.push_back(std::move(loweredFunction));
  }
  return true;
}

} // namespace primec
