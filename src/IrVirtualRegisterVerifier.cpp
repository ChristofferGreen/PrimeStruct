#include "primec/IrVirtualRegisterVerifier.h"

#include <algorithm>
#include <string>
#include <vector>

namespace primec {
namespace {

bool sameInstructionShape(const IrVirtualRegisterInstruction &left, const IrVirtualRegisterInstruction &right) {
  return left.instruction.op == right.instruction.op && left.instruction.imm == right.instruction.imm &&
         left.instruction.debugId == right.instruction.debugId && left.useRegisters == right.useRegisters &&
         left.defRegisters == right.defRegisters;
}

bool sameEdgeShape(const IrVirtualRegisterEdge &left, const IrVirtualRegisterEdge &right) {
  return left.successorBlockIndex == right.successorBlockIndex && left.stackMoves == right.stackMoves;
}

bool verifyFunctionAllocation(const IrVirtualRegisterFunction &function,
                              const IrLinearScanFunctionAllocation &allocation,
                              std::string &error) {
  error.clear();
  if (function.name != allocation.functionName) {
    error = "function name mismatch";
    return false;
  }

  std::vector<bool> seen(function.nextVirtualRegister, false);
  for (const auto &assignment : allocation.assignments) {
    if (assignment.virtualRegister >= function.nextVirtualRegister) {
      error = "allocation contains out-of-range virtual register";
      return false;
    }
    if (seen[assignment.virtualRegister]) {
      error = "allocation contains duplicate virtual register assignment";
      return false;
    }
    seen[assignment.virtualRegister] = true;
    if (assignment.endPosition < assignment.startPosition) {
      error = "allocation contains invalid interval range";
      return false;
    }
    if (assignment.spilled && assignment.spillSlot >= allocation.spillSlotCount) {
      error = "allocation spill slot exceeds spill slot count";
      return false;
    }
  }

  for (uint32_t reg = 0; reg < function.nextVirtualRegister; ++reg) {
    if (!seen[reg]) {
      error = "allocation missing virtual register assignment";
      return false;
    }
  }
  return true;
}

bool verifyBarrierOrdering(const IrVirtualRegisterBlock &originalBlock,
                           const std::vector<size_t> &positionByOriginalIndex,
                           std::string &error) {
  error.clear();
  const size_t blockInstructionCount = originalBlock.instructions.size();
  auto isBarrier = [](IrOpcode op) {
    switch (op) {
      case IrOpcode::Jump:
      case IrOpcode::JumpIfZero:
      case IrOpcode::Call:
      case IrOpcode::CallVoid:
      case IrOpcode::ReturnVoid:
      case IrOpcode::ReturnI32:
      case IrOpcode::ReturnI64:
      case IrOpcode::ReturnF32:
      case IrOpcode::ReturnF64:
      case IrOpcode::PrintI32:
      case IrOpcode::PrintI64:
      case IrOpcode::PrintU64:
      case IrOpcode::PrintString:
      case IrOpcode::PrintStringDynamic:
      case IrOpcode::PrintArgv:
      case IrOpcode::PrintArgvUnsafe:
      case IrOpcode::StoreLocal:
      case IrOpcode::HeapAlloc:
      case IrOpcode::HeapFree:
      case IrOpcode::HeapRealloc:
      case IrOpcode::LoadIndirect:
      case IrOpcode::StoreIndirect:
      case IrOpcode::FileOpenRead:
      case IrOpcode::FileOpenWrite:
      case IrOpcode::FileOpenAppend:
      case IrOpcode::FileOpenReadDynamic:
      case IrOpcode::FileOpenWriteDynamic:
      case IrOpcode::FileOpenAppendDynamic:
      case IrOpcode::FileReadByte:
      case IrOpcode::FileClose:
      case IrOpcode::FileFlush:
      case IrOpcode::FileWriteI32:
      case IrOpcode::FileWriteI64:
      case IrOpcode::FileWriteU64:
      case IrOpcode::FileWriteString:
      case IrOpcode::FileWriteByte:
      case IrOpcode::FileWriteNewline:
        return true;
      default:
        return false;
    }
  };

  for (size_t localIndex = 0; localIndex < blockInstructionCount; ++localIndex) {
    if (!isBarrier(originalBlock.instructions[localIndex].instruction.op)) {
      continue;
    }
    const size_t barrierPosition = positionByOriginalIndex[localIndex];
    for (size_t prior = 0; prior < localIndex; ++prior) {
      if (positionByOriginalIndex[prior] > barrierPosition) {
        error = "barrier order violated by schedule";
        return false;
      }
    }
    for (size_t after = localIndex + 1; after < blockInstructionCount; ++after) {
      if (positionByOriginalIndex[after] < barrierPosition) {
        error = "barrier order violated by schedule";
        return false;
      }
    }
  }

  return true;
}

bool verifyScheduledBlock(const IrVirtualRegisterFunction &function,
                         const IrVirtualRegisterBlock &originalBlock,
                         const IrVirtualRegisterScheduledBlock &scheduledBlock,
                         std::string &error) {
  error.clear();
  if (originalBlock.startInstructionIndex != scheduledBlock.startInstructionIndex ||
      originalBlock.endInstructionIndex != scheduledBlock.endInstructionIndex ||
      originalBlock.reachable != scheduledBlock.reachable) {
    error = "scheduled block metadata mismatch";
    return false;
  }
  if (originalBlock.successorEdges.size() != scheduledBlock.successorEdges.size()) {
    error = "scheduled block successor count mismatch";
    return false;
  }
  for (size_t edgeIndex = 0; edgeIndex < originalBlock.successorEdges.size(); ++edgeIndex) {
    if (!sameEdgeShape(originalBlock.successorEdges[edgeIndex], scheduledBlock.successorEdges[edgeIndex])) {
      error = "branch-edge value agreement mismatch";
      return false;
    }
  }
  if (originalBlock.instructions.size() != scheduledBlock.instructions.size()) {
    error = "scheduled block instruction count mismatch";
    return false;
  }

  const size_t blockInstructionCount = originalBlock.instructions.size();
  const size_t blockStart = originalBlock.startInstructionIndex;
  std::vector<size_t> positionByOriginalIndex(blockInstructionCount, blockInstructionCount);
  std::vector<size_t> countByOriginalIndex(blockInstructionCount, 0);
  std::vector<bool> available(function.nextVirtualRegister, false);
  for (uint32_t entryReg : originalBlock.entryRegisters) {
    if (entryReg >= function.nextVirtualRegister) {
      error = "scheduled block entry register out of range";
      return false;
    }
    available[entryReg] = true;
  }

  for (size_t scheduledPosition = 0; scheduledPosition < scheduledBlock.instructions.size(); ++scheduledPosition) {
    const auto &scheduledInstruction = scheduledBlock.instructions[scheduledPosition];
    if (scheduledInstruction.originalInstructionIndex < blockStart ||
        scheduledInstruction.originalInstructionIndex >= blockStart + blockInstructionCount) {
      error = "scheduled instruction index out of block range";
      return false;
    }
    const size_t localIndex = scheduledInstruction.originalInstructionIndex - blockStart;
    countByOriginalIndex[localIndex] += 1;
    positionByOriginalIndex[localIndex] = scheduledPosition;

    const auto &originalInstruction = originalBlock.instructions[localIndex];
    if (!sameInstructionShape(originalInstruction, scheduledInstruction.instruction)) {
      error = "scheduled instruction shape mismatch";
      return false;
    }

    for (uint32_t useReg : scheduledInstruction.instruction.useRegisters) {
      if (useReg >= function.nextVirtualRegister) {
        error = "scheduled instruction uses out-of-range register";
        return false;
      }
      if (!available[useReg]) {
        error = "scheduled instruction uses register before definition";
        return false;
      }
    }
    for (uint32_t defReg : scheduledInstruction.instruction.defRegisters) {
      if (defReg >= function.nextVirtualRegister) {
        error = "scheduled instruction defines out-of-range register";
        return false;
      }
      available[defReg] = true;
    }

    for (size_t dependencyIndex : scheduledInstruction.dependencyInstructionIndices) {
      if (dependencyIndex < blockStart || dependencyIndex >= blockStart + blockInstructionCount) {
        error = "scheduled instruction dependency out of block range";
        return false;
      }
      const size_t dependencyLocal = dependencyIndex - blockStart;
      if (positionByOriginalIndex[dependencyLocal] >= scheduledPosition) {
        error = "scheduled dependency appears after dependent instruction";
        return false;
      }
    }
  }

  for (size_t localIndex = 0; localIndex < blockInstructionCount; ++localIndex) {
    if (countByOriginalIndex[localIndex] != 1) {
      error = "schedule does not preserve one-to-one instruction mapping";
      return false;
    }
  }

  if (!verifyBarrierOrdering(originalBlock, positionByOriginalIndex, error)) {
    return false;
  }

  return true;
}

} // namespace

bool verifyIrVirtualRegisterScheduleAndAllocation(const IrVirtualRegisterModule &module,
                                                  const IrLinearScanModuleAllocation &allocation,
                                                  const IrVirtualRegisterScheduledModule &scheduled,
                                                  std::string &error) {
  error.clear();
  if (module.functions.size() != allocation.functions.size() || module.functions.size() != scheduled.functions.size()) {
    error = "verifier requires matching function counts";
    return false;
  }

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const auto &function = module.functions[functionIndex];
    const auto &functionAllocation = allocation.functions[functionIndex];
    const auto &scheduledFunction = scheduled.functions[functionIndex];

    if (function.name != scheduledFunction.functionName) {
      error = "scheduled function name mismatch";
      return false;
    }
    if (!verifyFunctionAllocation(function, functionAllocation, error)) {
      error = "allocation verification failed in function " + function.name + ": " + error;
      return false;
    }
    if (function.blocks.size() != scheduledFunction.blocks.size()) {
      error = "scheduled function block count mismatch";
      return false;
    }

    for (size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex) {
      if (!verifyScheduledBlock(function, function.blocks[blockIndex], scheduledFunction.blocks[blockIndex], error)) {
        error = "schedule verification failed in function " + function.name + " block " + std::to_string(blockIndex) +
                ": " + error;
        return false;
      }
    }
  }
  return true;
}

} // namespace primec
