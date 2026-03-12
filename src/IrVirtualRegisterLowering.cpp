#include "primec/IrVirtualRegisterLowering.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace primec {
namespace {

struct StackEffect {
  uint8_t pops = 0;
  uint8_t pushes = 0;
  uint8_t readsWithoutPop = 0;
};

struct BlockBuildInfo {
  size_t start = 0;
  size_t end = 0;
  std::vector<size_t> successors;
  std::vector<size_t> predecessors;
  int64_t entryDepth = std::numeric_limits<int64_t>::min();
  int64_t exitDepth = std::numeric_limits<int64_t>::min();
};

bool isReturnOpcode(IrOpcode op) {
  return op == IrOpcode::ReturnVoid || op == IrOpcode::ReturnI32 || op == IrOpcode::ReturnI64 ||
         op == IrOpcode::ReturnF32 || op == IrOpcode::ReturnF64;
}

bool isTerminatorOpcode(IrOpcode op) {
  return op == IrOpcode::Jump || op == IrOpcode::JumpIfZero || isReturnOpcode(op);
}

bool stackEffectForOpcode(IrOpcode op, StackEffect &out, std::string &error) {
  error.clear();
  switch (op) {
    case IrOpcode::PushI32:
    case IrOpcode::PushI64:
    case IrOpcode::PushF32:
    case IrOpcode::PushF64:
    case IrOpcode::PushArgc:
    case IrOpcode::LoadLocal:
    case IrOpcode::AddressOfLocal:
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::Call:
      out = {0, 1, 0};
      return true;
    case IrOpcode::StoreLocal:
    case IrOpcode::Pop:
    case IrOpcode::PrintI32:
    case IrOpcode::PrintI64:
    case IrOpcode::PrintU64:
    case IrOpcode::PrintStringDynamic:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
    case IrOpcode::JumpIfZero:
    case IrOpcode::ReturnI32:
    case IrOpcode::ReturnI64:
    case IrOpcode::ReturnF32:
    case IrOpcode::ReturnF64:
      out = {1, 0, 0};
      return true;
    case IrOpcode::LoadIndirect:
    case IrOpcode::HeapAlloc:
    case IrOpcode::NegI32:
    case IrOpcode::NegI64:
    case IrOpcode::NegF32:
    case IrOpcode::NegF64:
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF32:
    case IrOpcode::ConvertU64ToF64:
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
    case IrOpcode::FileClose:
    case IrOpcode::FileFlush:
    case IrOpcode::FileWriteString:
    case IrOpcode::FileWriteNewline:
    case IrOpcode::LoadStringByte:
    case IrOpcode::LoadStringLength:
      out = {1, 1, 0};
      return true;
    case IrOpcode::StoreIndirect:
    case IrOpcode::AddI32:
    case IrOpcode::SubI32:
    case IrOpcode::MulI32:
    case IrOpcode::DivI32:
    case IrOpcode::AddI64:
    case IrOpcode::SubI64:
    case IrOpcode::MulI64:
    case IrOpcode::DivI64:
    case IrOpcode::DivU64:
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32:
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64:
    case IrOpcode::CmpEqI32:
    case IrOpcode::CmpNeI32:
    case IrOpcode::CmpLtI32:
    case IrOpcode::CmpLeI32:
    case IrOpcode::CmpGtI32:
    case IrOpcode::CmpGeI32:
    case IrOpcode::CmpEqI64:
    case IrOpcode::CmpNeI64:
    case IrOpcode::CmpLtI64:
    case IrOpcode::CmpLeI64:
    case IrOpcode::CmpGtI64:
    case IrOpcode::CmpGeI64:
    case IrOpcode::CmpLtU64:
    case IrOpcode::CmpLeU64:
    case IrOpcode::CmpGtU64:
    case IrOpcode::CmpGeU64:
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32:
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64:
    case IrOpcode::FileWriteI32:
    case IrOpcode::FileWriteI64:
    case IrOpcode::FileWriteU64:
    case IrOpcode::FileWriteByte:
      out = {2, 1, 0};
      return true;
    case IrOpcode::Dup:
      out = {0, 1, 1};
      return true;
    case IrOpcode::Jump:
    case IrOpcode::ReturnVoid:
    case IrOpcode::PrintString:
    case IrOpcode::CallVoid:
      out = {0, 0, 0};
      return true;
    default:
      error = "unsupported opcode in virtual-register lowering";
      return false;
  }
}

std::vector<size_t> computeBlockLeaders(const IrFunction &function, std::string &error) {
  std::vector<size_t> leaders;
  if (function.instructions.empty()) {
    return leaders;
  }

  leaders.push_back(0);
  const size_t instructionCount = function.instructions.size();
  for (size_t index = 0; index < instructionCount; ++index) {
    const IrInstruction &inst = function.instructions[index];
    if (inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) {
      if (inst.imm > instructionCount) {
        error = "virtual-register lowering found invalid jump target";
        return {};
      }
      if (inst.imm < instructionCount) {
        leaders.push_back(static_cast<size_t>(inst.imm));
      }
    }
    if (isTerminatorOpcode(inst.op) && index + 1 < instructionCount) {
      leaders.push_back(index + 1);
    }
  }

  std::sort(leaders.begin(), leaders.end());
  leaders.erase(std::unique(leaders.begin(), leaders.end()), leaders.end());
  return leaders;
}

size_t findBlockIndexForInstruction(const std::vector<size_t> &leaders, size_t instructionIndex) {
  const auto it = std::upper_bound(leaders.begin(), leaders.end(), instructionIndex);
  if (it == leaders.begin()) {
    return 0;
  }
  return static_cast<size_t>(std::distance(leaders.begin(), it - 1));
}

void addSuccessorUnique(BlockBuildInfo &block, size_t successor) {
  if (std::find(block.successors.begin(), block.successors.end(), successor) == block.successors.end()) {
    block.successors.push_back(successor);
  }
}

bool buildBlockGraph(const IrFunction &function,
                     const std::vector<size_t> &leaders,
                     std::vector<BlockBuildInfo> &blocks,
                     std::string &error) {
  error.clear();
  blocks.clear();
  if (leaders.empty()) {
    return true;
  }

  blocks.resize(leaders.size());
  for (size_t blockIndex = 0; blockIndex < leaders.size(); ++blockIndex) {
    BlockBuildInfo &block = blocks[blockIndex];
    block.start = leaders[blockIndex];
    block.end = (blockIndex + 1 < leaders.size()) ? leaders[blockIndex + 1] : function.instructions.size();
  }

  for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    BlockBuildInfo &block = blocks[blockIndex];
    if (block.start == block.end) {
      if (blockIndex + 1 < blocks.size()) {
        addSuccessorUnique(block, blockIndex + 1);
      }
      continue;
    }

    const IrInstruction &lastInst = function.instructions[block.end - 1];
    if (lastInst.op == IrOpcode::Jump || lastInst.op == IrOpcode::JumpIfZero) {
      if (lastInst.imm > function.instructions.size()) {
        error = "virtual-register lowering found invalid jump target";
        return false;
      }
      if (lastInst.imm < function.instructions.size()) {
        addSuccessorUnique(block, findBlockIndexForInstruction(leaders, static_cast<size_t>(lastInst.imm)));
      }
      if (lastInst.op == IrOpcode::JumpIfZero && blockIndex + 1 < blocks.size()) {
        addSuccessorUnique(block, blockIndex + 1);
      }
      continue;
    }

    if (!isReturnOpcode(lastInst.op) && blockIndex + 1 < blocks.size()) {
      addSuccessorUnique(block, blockIndex + 1);
    }
  }

  for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    for (size_t successor : blocks[blockIndex].successors) {
      blocks[successor].predecessors.push_back(blockIndex);
    }
  }

  return true;
}

bool propagateReachableStackDepths(const IrFunction &function, std::vector<BlockBuildInfo> &blocks, std::string &error) {
  error.clear();
  if (blocks.empty()) {
    return true;
  }

  std::vector<size_t> worklist;
  blocks[0].entryDepth = 0;
  worklist.push_back(0);

  while (!worklist.empty()) {
    const size_t blockIndex = worklist.back();
    worklist.pop_back();

    BlockBuildInfo &block = blocks[blockIndex];
    int64_t depth = block.entryDepth;
    if (depth < 0) {
      continue;
    }

    for (size_t instructionIndex = block.start; instructionIndex < block.end; ++instructionIndex) {
      StackEffect effect;
      if (!stackEffectForOpcode(function.instructions[instructionIndex].op, effect, error)) {
        return false;
      }
      if (static_cast<int64_t>(effect.pops) > depth) {
        error = "virtual-register lowering found stack underflow at instruction " + std::to_string(instructionIndex);
        return false;
      }
      if (effect.readsWithoutPop > 0 && depth < static_cast<int64_t>(effect.readsWithoutPop)) {
        error = "virtual-register lowering found invalid dup at instruction " + std::to_string(instructionIndex);
        return false;
      }
      depth -= static_cast<int64_t>(effect.pops);
      depth += static_cast<int64_t>(effect.pushes);
    }
    block.exitDepth = depth;

    for (size_t successor : block.successors) {
      BlockBuildInfo &successorBlock = blocks[successor];
      if (successorBlock.entryDepth == std::numeric_limits<int64_t>::min()) {
        successorBlock.entryDepth = depth;
        worklist.push_back(successor);
      } else if (successorBlock.entryDepth != depth) {
        error = "virtual-register lowering found inconsistent stack depth at block boundary";
        return false;
      }
    }
  }

  return true;
}

bool lowerFunctionToVirtualRegisters(const IrFunction &function, IrVirtualRegisterFunction &out, std::string &error) {
  error.clear();
  out = {};
  out.name = function.name;
  out.metadata = function.metadata;
  out.localDebugSlots = function.localDebugSlots;
  if (function.instructions.empty()) {
    return true;
  }

  std::vector<size_t> leaders = computeBlockLeaders(function, error);
  if (!error.empty()) {
    return false;
  }

  std::vector<BlockBuildInfo> blockInfo;
  if (!buildBlockGraph(function, leaders, blockInfo, error)) {
    return false;
  }
  if (!propagateReachableStackDepths(function, blockInfo, error)) {
    return false;
  }

  out.blocks.resize(blockInfo.size());

  uint32_t nextRegisterId = 0;
  for (size_t blockIndex = 0; blockIndex < blockInfo.size(); ++blockIndex) {
    const BlockBuildInfo &inBlock = blockInfo[blockIndex];
    IrVirtualRegisterBlock &outBlock = out.blocks[blockIndex];
    outBlock.startInstructionIndex = inBlock.start;
    outBlock.endInstructionIndex = inBlock.end;
    outBlock.reachable = inBlock.entryDepth != std::numeric_limits<int64_t>::min();

    if (outBlock.reachable) {
      const size_t entrySize = static_cast<size_t>(inBlock.entryDepth);
      outBlock.entryRegisters.reserve(entrySize);
      for (size_t slot = 0; slot < entrySize; ++slot) {
        outBlock.entryRegisters.push_back(nextRegisterId++);
      }
    }
  }

  for (size_t blockIndex = 0; blockIndex < blockInfo.size(); ++blockIndex) {
    const BlockBuildInfo &inBlock = blockInfo[blockIndex];
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

      StackEffect effect;
      if (!stackEffectForOpcode(loweredInstruction.instruction.op, effect, error)) {
        return false;
      }
      if (stack.size() < static_cast<size_t>(effect.pops)) {
        error = "virtual-register lowering found stack underflow at instruction " + std::to_string(instructionIndex);
        return false;
      }
      if (effect.readsWithoutPop > 0 && stack.size() < static_cast<size_t>(effect.readsWithoutPop)) {
        error = "virtual-register lowering found invalid dup at instruction " + std::to_string(instructionIndex);
        return false;
      }

      for (uint8_t read = 0; read < effect.readsWithoutPop; ++read) {
        loweredInstruction.useRegisters.push_back(stack[stack.size() - static_cast<size_t>(read) - 1]);
      }

      std::vector<uint32_t> poppedRegisters;
      poppedRegisters.reserve(effect.pops);
      for (uint8_t pop = 0; pop < effect.pops; ++pop) {
        poppedRegisters.push_back(stack.back());
        stack.pop_back();
      }
      for (auto it = poppedRegisters.rbegin(); it != poppedRegisters.rend(); ++it) {
        loweredInstruction.useRegisters.push_back(*it);
      }

      loweredInstruction.defRegisters.reserve(effect.pushes);
      for (uint8_t push = 0; push < effect.pushes; ++push) {
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
    if (!lowerFunctionToVirtualRegisters(module.functions[functionIndex], out.functions[functionIndex], error)) {
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
