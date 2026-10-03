#include "primec/ir/IrVirtualRegisterLowering.h"

#include "primec/ir/IrCfg.h"

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

bool lowerFunctionToVirtualRegisters(const IrFunction &function,
                                     const IrModule &module,
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
    }
  }

  for (size_t blockIndex = 0; blockIndex < cfg.blocks.size(); ++blockIndex) {
    const IrCfgBlock &inBlock = cfg.blocks[blockIndex];
    IrVirtualRegisterBlock &outBlock = out.blocks[blockIndex];
    std::vector<uint32_t> stack = outBlock.entryRegisters;

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

      outBlock.instructions.push_back(std::move(loweredInstruction));
    }

    outBlock.exitRegisters = stack;
    if (!outBlock.reachable) {
      continue;
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
      outBlock.successorEdges.push_back(std::move(edge));
    }
  }

  out.nextVirtualRegister = nextRegisterId;
  return true;
}

} // namespace

bool lowerIrModuleToBlockVirtualRegisters(const IrModule &module, IrVirtualRegisterModule &out, std::string &error) {
  error.clear();
  out = {};
  out.entryIndex = module.entryIndex;
  out.stringTable = module.stringTable;
  out.structLayouts = module.structLayouts;
  out.instructionSourceMap = module.instructionSourceMap;
  out.functions.resize(module.functions.size());

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    if (!lowerFunctionToVirtualRegisters(module.functions[functionIndex], module, out.functions[functionIndex], error)) {
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
