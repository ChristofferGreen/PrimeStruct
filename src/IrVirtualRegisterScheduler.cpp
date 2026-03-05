#include "primec/IrVirtualRegisterScheduler.h"

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace primec {
namespace {

struct AssignmentLookupEntry {
  bool spilled = false;
};

struct ScheduledCandidate {
  size_t localInstructionIndex = 0;
  uint32_t latencyScore = 0;
};

bool buildAssignmentLookup(const IrLinearScanFunctionAllocation &allocation,
                           std::vector<std::optional<AssignmentLookupEntry>> &lookup,
                           std::string &error) {
  error.clear();
  uint32_t maxVirtualRegister = 0;
  for (const auto &assignment : allocation.assignments) {
    maxVirtualRegister = std::max(maxVirtualRegister, assignment.virtualRegister);
  }
  lookup.clear();
  lookup.resize(allocation.assignments.empty() ? 0 : static_cast<size_t>(maxVirtualRegister + 1));
  for (const auto &assignment : allocation.assignments) {
    lookup[assignment.virtualRegister] = AssignmentLookupEntry{assignment.spilled};
  }
  return true;
}

bool isRegisterSpilled(const std::vector<std::optional<AssignmentLookupEntry>> &lookup, uint32_t virtualRegister) {
  if (virtualRegister >= lookup.size() || !lookup[virtualRegister].has_value()) {
    return false;
  }
  return lookup[virtualRegister]->spilled;
}

uint32_t opcodeBaseLatency(IrOpcode op) {
  switch (op) {
    case IrOpcode::DivI32:
    case IrOpcode::DivI64:
    case IrOpcode::DivU64:
    case IrOpcode::DivF32:
    case IrOpcode::DivF64:
      return 8;
    case IrOpcode::MulI32:
    case IrOpcode::MulI64:
    case IrOpcode::MulF32:
    case IrOpcode::MulF64:
      return 4;
    case IrOpcode::AddI32:
    case IrOpcode::SubI32:
    case IrOpcode::AddI64:
    case IrOpcode::SubI64:
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
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
      return 2;
    default:
      return 1;
  }
}

bool isSchedulingBarrierOpcode(IrOpcode op) {
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
    case IrOpcode::LoadIndirect:
    case IrOpcode::StoreIndirect:
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
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
}

void addEdge(size_t from,
             size_t to,
             std::vector<std::vector<size_t>> &successors,
             std::vector<size_t> &inDegree) {
  if (from == to) {
    return;
  }
  auto &succ = successors[from];
  if (std::find(succ.begin(), succ.end(), to) != succ.end()) {
    return;
  }
  succ.push_back(to);
  inDegree[to] += 1;
}

bool buildBlockSchedule(const IrVirtualRegisterBlock &block,
                        size_t blockStartInstructionIndex,
                        const std::vector<std::optional<AssignmentLookupEntry>> &assignmentLookup,
                        IrVirtualRegisterScheduledBlock &out,
                        std::string &error) {
  error.clear();
  out = {};
  out.startInstructionIndex = block.startInstructionIndex;
  out.endInstructionIndex = block.endInstructionIndex;
  out.reachable = block.reachable;
  out.successorEdges = block.successorEdges;

  const size_t instructionCount = block.instructions.size();
  if (instructionCount == 0) {
    return true;
  }

  std::vector<uint32_t> latencyScores(instructionCount, 0);
  for (size_t index = 0; index < instructionCount; ++index) {
    const auto &instruction = block.instructions[index];
    uint32_t spillPenalty = 0;
    for (uint32_t useReg : instruction.useRegisters) {
      if (isRegisterSpilled(assignmentLookup, useReg)) {
        spillPenalty += 2;
      }
    }
    for (uint32_t defReg : instruction.defRegisters) {
      if (isRegisterSpilled(assignmentLookup, defReg)) {
        spillPenalty += 2;
      }
    }
    latencyScores[index] = opcodeBaseLatency(instruction.instruction.op) + spillPenalty;
  }

  std::vector<std::vector<size_t>> successors(instructionCount);
  std::vector<size_t> inDegree(instructionCount, 0);

  std::vector<std::optional<size_t>> lastDefByRegister(assignmentLookup.size());
  for (size_t index = 0; index < instructionCount; ++index) {
    const auto &instruction = block.instructions[index];
    for (uint32_t useReg : instruction.useRegisters) {
      if (useReg < lastDefByRegister.size() && lastDefByRegister[useReg].has_value()) {
        addEdge(lastDefByRegister[useReg].value(), index, successors, inDegree);
      }
    }
    for (uint32_t defReg : instruction.defRegisters) {
      if (defReg >= lastDefByRegister.size()) {
        lastDefByRegister.resize(static_cast<size_t>(defReg) + 1);
      }
      lastDefByRegister[defReg] = index;
    }
  }

  std::optional<size_t> lastBarrier;
  for (size_t index = 0; index < instructionCount; ++index) {
    const bool isBarrier = isSchedulingBarrierOpcode(block.instructions[index].instruction.op);
    if (isBarrier) {
      for (size_t prior = 0; prior < index; ++prior) {
        addEdge(prior, index, successors, inDegree);
      }
      lastBarrier = index;
      continue;
    }
    if (lastBarrier.has_value()) {
      addEdge(lastBarrier.value(), index, successors, inDegree);
    }
  }

  std::vector<ScheduledCandidate> ready;
  ready.reserve(instructionCount);
  for (size_t index = 0; index < instructionCount; ++index) {
    if (inDegree[index] == 0) {
      ready.push_back({index, latencyScores[index]});
    }
  }

  std::vector<size_t> orderedLocalIndices;
  orderedLocalIndices.reserve(instructionCount);
  while (!ready.empty()) {
    auto pickIt = ready.begin();
    for (auto it = ready.begin() + 1; it != ready.end(); ++it) {
      if (it->latencyScore > pickIt->latencyScore) {
        pickIt = it;
        continue;
      }
      if (it->latencyScore == pickIt->latencyScore && it->localInstructionIndex < pickIt->localInstructionIndex) {
        pickIt = it;
      }
    }

    const size_t selected = pickIt->localInstructionIndex;
    ready.erase(pickIt);
    orderedLocalIndices.push_back(selected);

    for (size_t successor : successors[selected]) {
      if (inDegree[successor] == 0) {
        error = "scheduler encountered invalid dependency graph";
        return false;
      }
      inDegree[successor] -= 1;
      if (inDegree[successor] == 0) {
        ready.push_back({successor, latencyScores[successor]});
      }
    }
  }

  if (orderedLocalIndices.size() != instructionCount) {
    error = "scheduler found cyclic dependencies in block";
    return false;
  }

  out.instructions.reserve(instructionCount);
  for (size_t localIndex : orderedLocalIndices) {
    IrVirtualRegisterScheduledInstruction scheduledInstruction;
    scheduledInstruction.instruction = block.instructions[localIndex];
    scheduledInstruction.originalInstructionIndex = blockStartInstructionIndex + localIndex;
    scheduledInstruction.latencyScore = latencyScores[localIndex];
    for (size_t dependencyLocalIndex = 0; dependencyLocalIndex < instructionCount; ++dependencyLocalIndex) {
      if (std::find(successors[dependencyLocalIndex].begin(), successors[dependencyLocalIndex].end(), localIndex) !=
          successors[dependencyLocalIndex].end()) {
        scheduledInstruction.dependencyInstructionIndices.push_back(blockStartInstructionIndex + dependencyLocalIndex);
      }
    }
    std::sort(scheduledInstruction.dependencyInstructionIndices.begin(),
              scheduledInstruction.dependencyInstructionIndices.end());
    out.instructions.push_back(std::move(scheduledInstruction));
  }

  return true;
}

} // namespace

bool scheduleIrVirtualRegisters(const IrVirtualRegisterModule &module,
                                const IrLinearScanModuleAllocation &allocation,
                                IrVirtualRegisterScheduledModule &out,
                                std::string &error) {
  error.clear();
  out = {};
  if (module.functions.size() != allocation.functions.size()) {
    error = "scheduler requires matching function counts";
    return false;
  }

  out.functions.resize(module.functions.size());
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const auto &function = module.functions[functionIndex];
    const auto &functionAllocation = allocation.functions[functionIndex];
    if (function.name != functionAllocation.functionName) {
      error = "scheduler requires matching function names";
      return false;
    }

    std::vector<std::optional<AssignmentLookupEntry>> assignmentLookup;
    if (!buildAssignmentLookup(functionAllocation, assignmentLookup, error)) {
      return false;
    }

    auto &scheduledFunction = out.functions[functionIndex];
    scheduledFunction.functionName = function.name;
    scheduledFunction.blocks.resize(function.blocks.size());

    for (size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex) {
      if (!buildBlockSchedule(function.blocks[blockIndex],
                              function.blocks[blockIndex].startInstructionIndex,
                              assignmentLookup,
                              scheduledFunction.blocks[blockIndex],
                              error)) {
        if (!error.empty()) {
          error = "scheduler failed in function " + function.name + " block " + std::to_string(blockIndex) + ": " +
                  error;
        }
        return false;
      }
    }
  }

  return true;
}

} // namespace primec
