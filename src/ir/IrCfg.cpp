#include "primec/ir/IrCfg.h"

#include <algorithm>

namespace primec {
namespace {

bool isReturnOpcode(IrOpcode op) {
  return op == IrOpcode::ReturnVoid || op == IrOpcode::ReturnI32 || op == IrOpcode::ReturnI64 ||
         op == IrOpcode::ReturnF32 || op == IrOpcode::ReturnF64;
}

bool isJumpOpcode(IrOpcode op) {
  return op == IrOpcode::Jump || op == IrOpcode::JumpIfZero;
}

bool isTerminatorOpcode(IrOpcode op) {
  return isJumpOpcode(op) || isReturnOpcode(op);
}

void addSuccessorUnique(IrCfgBlock &block, size_t successor) {
  if (std::find(block.successors.begin(), block.successors.end(), successor) ==
      block.successors.end()) {
    block.successors.push_back(successor);
  }
}

IrCfgError makeError(IrCfgErrorKind kind, size_t instructionIndex, IrOpcode opcode) {
  IrCfgError error;
  error.kind = kind;
  error.instructionIndex = instructionIndex;
  error.opcode = opcode;
  return error;
}

} // namespace

bool computeIrStackEffect(const IrInstruction &instruction,
                          const IrModule &module,
                          IrStackEffect &out) {
  const auto set = [&out](uint32_t pops, uint32_t pushes, uint32_t readsWithoutPop = 0) {
    out = {pops, pushes, readsWithoutPop};
    return true;
  };
  switch (instruction.op) {
  // Produce one value.
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
    return set(0, 1);
  // Consume one value.
  case IrOpcode::StoreLocal:
  case IrOpcode::Pop:
  case IrOpcode::HeapFree:
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
    return set(1, 0);
  // Replace the top value with a new one.
  case IrOpcode::FileOpenReadDynamic:
  case IrOpcode::FileOpenWriteDynamic:
  case IrOpcode::FileOpenAppendDynamic:
  case IrOpcode::LoadIndirect:
  case IrOpcode::HeapAlloc:
  case IrOpcode::SextI32:
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
  case IrOpcode::FileReadByte:
  case IrOpcode::FileWriteString:
  case IrOpcode::FileWriteNewline:
  case IrOpcode::LoadStringByte:
  case IrOpcode::LoadStringLength:
    return set(1, 1);
  // Combine two values into one.
  case IrOpcode::StoreIndirect:
  case IrOpcode::HeapRealloc:
  case IrOpcode::LoadStringByteDynamic:
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
  case IrOpcode::FileWriteStringDynamic:
  case IrOpcode::FileWriteByte:
    return set(2, 1);
  case IrOpcode::Dup:
    return set(0, 1, 1);
  case IrOpcode::Jump:
  case IrOpcode::ReturnVoid:
  case IrOpcode::PrintString:
    return set(0, 0);
  case IrOpcode::Call:
  case IrOpcode::CallVoid: {
    uint32_t parameterCount = 0;
    if (instruction.imm < module.functions.size()) {
      parameterCount = module.functions[static_cast<size_t>(instruction.imm)].parameterCount;
    }
    return set(parameterCount, instruction.op == IrOpcode::Call ? 1u : 0u);
  }
  case IrOpcode::CallHost: {
    uint32_t parameterCount = 0;
    uint32_t pushes = 0;
    if (instruction.imm < module.hostImports.size()) {
      const IrHostImport &import = module.hostImports[static_cast<size_t>(instruction.imm)];
      parameterCount = static_cast<uint32_t>(import.parameters.size());
      pushes = import.returnKind == IrHostValueKind::Void ? 0u : 1u;
    }
    return set(parameterCount, pushes);
  }
  }
  return false;
}

bool buildIrCfg(const IrFunction &function, const IrModule &module, IrCfg &out, IrCfgError &error) {
  out = {};
  error = {};
  const size_t instructionCount = function.instructions.size();
  if (instructionCount == 0) {
    return true;
  }

  // Block leaders.
  std::vector<size_t> leaders;
  leaders.push_back(0);
  for (size_t index = 0; index < instructionCount; ++index) {
    const IrInstruction &instruction = function.instructions[index];
    if (isJumpOpcode(instruction.op)) {
      if (instruction.imm > instructionCount) {
        error = makeError(IrCfgErrorKind::InvalidJumpTarget, index, instruction.op);
        return false;
      }
      if (instruction.imm < instructionCount) {
        leaders.push_back(static_cast<size_t>(instruction.imm));
      }
    }
    if (isTerminatorOpcode(instruction.op) && index + 1 < instructionCount) {
      leaders.push_back(index + 1);
    }
  }
  std::sort(leaders.begin(), leaders.end());
  leaders.erase(std::unique(leaders.begin(), leaders.end()), leaders.end());

  out.blocks.resize(leaders.size());
  for (size_t blockIndex = 0; blockIndex < leaders.size(); ++blockIndex) {
    IrCfgBlock &block = out.blocks[blockIndex];
    block.start = leaders[blockIndex];
    block.end = blockIndex + 1 < leaders.size() ? leaders[blockIndex + 1] : instructionCount;
  }

  // Edges.
  for (size_t blockIndex = 0; blockIndex < out.blocks.size(); ++blockIndex) {
    IrCfgBlock &block = out.blocks[blockIndex];
    const IrInstruction &last = function.instructions[block.end - 1];
    if (isJumpOpcode(last.op)) {
      if (last.imm < instructionCount) {
        addSuccessorUnique(block,
                           irCfgBlockIndexForInstruction(out, static_cast<size_t>(last.imm)));
      }
      if (last.op == IrOpcode::JumpIfZero && blockIndex + 1 < out.blocks.size()) {
        addSuccessorUnique(block, blockIndex + 1);
      }
      continue;
    }
    if (!isReturnOpcode(last.op) && blockIndex + 1 < out.blocks.size()) {
      addSuccessorUnique(block, blockIndex + 1);
    }
  }
  for (size_t blockIndex = 0; blockIndex < out.blocks.size(); ++blockIndex) {
    for (const size_t successor : out.blocks[blockIndex].successors) {
      out.blocks[successor].predecessors.push_back(blockIndex);
    }
  }

  // Operand-stack depth, propagated from the entry block.
  std::vector<size_t> worklist;
  out.blocks[0].reachable = true;
  out.blocks[0].entryDepth = static_cast<int64_t>(function.parameterCount);
  worklist.push_back(0);
  while (!worklist.empty()) {
    const size_t blockIndex = worklist.back();
    worklist.pop_back();
    IrCfgBlock &block = out.blocks[blockIndex];

    int64_t depth = block.entryDepth;
    block.maxDepth = depth;
    for (size_t index = block.start; index < block.end; ++index) {
      const IrInstruction &instruction = function.instructions[index];
      IrStackEffect effect;
      if (!computeIrStackEffect(instruction, module, effect)) {
        error = makeError(IrCfgErrorKind::UnsupportedOpcode, index, instruction.op);
        return false;
      }
      if (static_cast<int64_t>(effect.pops) > depth) {
        error = makeError(IrCfgErrorKind::StackUnderflow, index, instruction.op);
        return false;
      }
      if (effect.readsWithoutPop > 0 && depth < static_cast<int64_t>(effect.readsWithoutPop)) {
        error = makeError(IrCfgErrorKind::InvalidDup, index, instruction.op);
        return false;
      }
      depth -= static_cast<int64_t>(effect.pops);
      depth += static_cast<int64_t>(effect.pushes);
      block.maxDepth = std::max(block.maxDepth, depth);
    }
    block.exitDepth = depth;
    out.maxStackDepth = std::max(out.maxStackDepth, block.maxDepth);

    for (const size_t successor : block.successors) {
      IrCfgBlock &successorBlock = out.blocks[successor];
      if (!successorBlock.reachable) {
        successorBlock.reachable = true;
        successorBlock.entryDepth = depth;
        worklist.push_back(successor);
      } else if (successorBlock.entryDepth != depth) {
        error = makeError(IrCfgErrorKind::InconsistentDepth,
                          successorBlock.start,
                          function.instructions[successorBlock.start].op);
        return false;
      }
    }
  }
  return true;
}

size_t irCfgBlockIndexForInstruction(const IrCfg &cfg, size_t instructionIndex) {
  // Blocks are sorted by start; find the last block that starts at or before
  // the instruction.
  size_t low = 0;
  size_t high = cfg.blocks.size();
  while (low < high) {
    const size_t mid = low + (high - low) / 2;
    if (cfg.blocks[mid].start <= instructionIndex) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return low == 0 ? 0 : low - 1;
}

} // namespace primec
