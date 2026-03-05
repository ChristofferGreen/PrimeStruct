#include "primec/IrVirtualRegisterSpillInsertion.h"

#include <algorithm>
#include <optional>
#include <tuple>
#include <vector>

namespace primec {
namespace {

struct AssignmentLookupEntry {
  bool spilled = false;
  uint32_t spillSlot = 0;
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
    const size_t index = static_cast<size_t>(assignment.virtualRegister);
    if (index >= lookup.size()) {
      error = "spill insertion found out-of-range virtual register assignment";
      return false;
    }
    lookup[index] = AssignmentLookupEntry{assignment.spilled, assignment.spillSlot};
  }
  return true;
}

std::optional<uint32_t> findSpillSlot(const std::vector<std::optional<AssignmentLookupEntry>> &lookup,
                                      uint32_t virtualRegister) {
  if (virtualRegister >= lookup.size() || !lookup[virtualRegister].has_value()) {
    return std::nullopt;
  }
  const AssignmentLookupEntry &entry = lookup[virtualRegister].value();
  if (!entry.spilled) {
    return std::nullopt;
  }
  return entry.spillSlot;
}

void sortAndUniqueOps(std::vector<IrVirtualRegisterSpillOp> &ops) {
  std::sort(ops.begin(),
            ops.end(),
            [](const IrVirtualRegisterSpillOp &left, const IrVirtualRegisterSpillOp &right) {
              return std::tie(left.kind, left.virtualRegister, left.spillSlot) <
                     std::tie(right.kind, right.virtualRegister, right.spillSlot);
            });
  ops.erase(std::unique(ops.begin(),
                        ops.end(),
                        [](const IrVirtualRegisterSpillOp &left, const IrVirtualRegisterSpillOp &right) {
                          return left.kind == right.kind && left.virtualRegister == right.virtualRegister &&
                                 left.spillSlot == right.spillSlot;
                        }),
            ops.end());
}

std::vector<IrVirtualRegisterSpillOp> computeExpectedEdgeOps(
    const IrVirtualRegisterEdge &edge,
    const std::vector<std::optional<AssignmentLookupEntry>> &lookup) {
  std::vector<IrVirtualRegisterSpillOp> edgeOps;
  for (const auto &move : edge.stackMoves) {
    const std::optional<uint32_t> sourceSpill = findSpillSlot(lookup, move.sourceRegister);
    const std::optional<uint32_t> destinationSpill = findSpillSlot(lookup, move.destinationRegister);
    if (!sourceSpill.has_value() && !destinationSpill.has_value()) {
      continue;
    }
    if (sourceSpill.has_value() && !destinationSpill.has_value()) {
      edgeOps.push_back({IrVirtualRegisterSpillOpKind::ReloadFromSpill, move.destinationRegister, sourceSpill.value()});
      continue;
    }
    if (!sourceSpill.has_value() && destinationSpill.has_value()) {
      edgeOps.push_back({IrVirtualRegisterSpillOpKind::SpillToSlot, move.sourceRegister, destinationSpill.value()});
      continue;
    }
    if (sourceSpill.value() == destinationSpill.value()) {
      continue;
    }
    edgeOps.push_back({IrVirtualRegisterSpillOpKind::ReloadFromSpill, move.sourceRegister, sourceSpill.value()});
    edgeOps.push_back({IrVirtualRegisterSpillOpKind::SpillToSlot, move.sourceRegister, destinationSpill.value()});
  }
  sortAndUniqueOps(edgeOps);
  return edgeOps;
}

bool verifyInstructionOps(const IrVirtualRegisterInstruction &instruction,
                          const std::vector<IrVirtualRegisterSpillOp> &beforeOps,
                          const std::vector<IrVirtualRegisterSpillOp> &afterOps,
                          const std::vector<std::optional<AssignmentLookupEntry>> &lookup,
                          std::string &error) {
  error.clear();
  for (uint32_t useReg : instruction.useRegisters) {
    const std::optional<uint32_t> spillSlot = findSpillSlot(lookup, useReg);
    if (!spillSlot.has_value()) {
      continue;
    }
    const bool hasReload = std::any_of(beforeOps.begin(), beforeOps.end(), [&](const IrVirtualRegisterSpillOp &op) {
      return op.kind == IrVirtualRegisterSpillOpKind::ReloadFromSpill && op.virtualRegister == useReg &&
             op.spillSlot == spillSlot.value();
    });
    if (!hasReload) {
      error = "spill insertion verifier missing reload op for spilled use register";
      return false;
    }
  }
  for (uint32_t defReg : instruction.defRegisters) {
    const std::optional<uint32_t> spillSlot = findSpillSlot(lookup, defReg);
    if (!spillSlot.has_value()) {
      continue;
    }
    const bool hasSpill = std::any_of(afterOps.begin(), afterOps.end(), [&](const IrVirtualRegisterSpillOp &op) {
      return op.kind == IrVirtualRegisterSpillOpKind::SpillToSlot && op.virtualRegister == defReg &&
             op.spillSlot == spillSlot.value();
    });
    if (!hasSpill) {
      error = "spill insertion verifier missing spill op for spilled def register";
      return false;
    }
  }
  return true;
}

} // namespace

bool insertIrVirtualRegisterSpills(const IrVirtualRegisterModule &module,
                                   const IrLinearScanModuleAllocation &allocation,
                                   IrVirtualRegisterSpillPlan &out,
                                   std::string &error) {
  error.clear();
  out = {};
  if (module.functions.size() != allocation.functions.size()) {
    error = "spill insertion requires matching function counts";
    return false;
  }

  out.functions.resize(module.functions.size());
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const auto &function = module.functions[functionIndex];
    const auto &functionAllocation = allocation.functions[functionIndex];
    if (function.name != functionAllocation.functionName) {
      error = "spill insertion requires allocation function names to match virtual-register module";
      return false;
    }

    std::vector<std::optional<AssignmentLookupEntry>> assignmentLookup;
    if (!buildAssignmentLookup(functionAllocation, assignmentLookup, error)) {
      return false;
    }

    auto &plannedFunction = out.functions[functionIndex];
    plannedFunction.functionName = function.name;
    plannedFunction.spillSlotCount = functionAllocation.spillSlotCount;
    plannedFunction.blocks.resize(function.blocks.size());

    for (size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex) {
      const auto &block = function.blocks[blockIndex];
      auto &plannedBlock = plannedFunction.blocks[blockIndex];
      plannedBlock.startInstructionIndex = block.startInstructionIndex;
      plannedBlock.endInstructionIndex = block.endInstructionIndex;
      plannedBlock.reachable = block.reachable;
      plannedBlock.instructions.resize(block.instructions.size());

      for (size_t instructionIndex = 0; instructionIndex < block.instructions.size(); ++instructionIndex) {
        const auto &instruction = block.instructions[instructionIndex];
        auto &plannedInstruction = plannedBlock.instructions[instructionIndex];
        plannedInstruction.instruction = instruction;

        for (uint32_t useReg : instruction.useRegisters) {
          const std::optional<uint32_t> spillSlot = findSpillSlot(assignmentLookup, useReg);
          if (!spillSlot.has_value()) {
            continue;
          }
          plannedInstruction.beforeInstructionOps.push_back(
              {IrVirtualRegisterSpillOpKind::ReloadFromSpill, useReg, spillSlot.value()});
        }
        for (uint32_t defReg : instruction.defRegisters) {
          const std::optional<uint32_t> spillSlot = findSpillSlot(assignmentLookup, defReg);
          if (!spillSlot.has_value()) {
            continue;
          }
          plannedInstruction.afterInstructionOps.push_back(
              {IrVirtualRegisterSpillOpKind::SpillToSlot, defReg, spillSlot.value()});
        }
        sortAndUniqueOps(plannedInstruction.beforeInstructionOps);
        sortAndUniqueOps(plannedInstruction.afterInstructionOps);
      }

      plannedBlock.successorEdges.resize(block.successorEdges.size());
      for (size_t edgeIndex = 0; edgeIndex < block.successorEdges.size(); ++edgeIndex) {
        const auto &edge = block.successorEdges[edgeIndex];
        auto &plannedEdge = plannedBlock.successorEdges[edgeIndex];
        plannedEdge.successorBlockIndex = edge.successorBlockIndex;
        plannedEdge.edgeOps = computeExpectedEdgeOps(edge, assignmentLookup);
      }
    }
  }

  return true;
}

bool verifyIrVirtualRegisterSpillPlan(const IrVirtualRegisterModule &module,
                                      const IrLinearScanModuleAllocation &allocation,
                                      const IrVirtualRegisterSpillPlan &plan,
                                      std::string &error) {
  error.clear();
  if (module.functions.size() != allocation.functions.size() || module.functions.size() != plan.functions.size()) {
    error = "spill insertion verifier requires matching function counts";
    return false;
  }

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const auto &function = module.functions[functionIndex];
    const auto &functionAllocation = allocation.functions[functionIndex];
    const auto &plannedFunction = plan.functions[functionIndex];
    if (function.name != functionAllocation.functionName || function.name != plannedFunction.functionName) {
      error = "spill insertion verifier requires matching function names";
      return false;
    }
    if (function.blocks.size() != plannedFunction.blocks.size()) {
      error = "spill insertion verifier requires matching block counts";
      return false;
    }

    std::vector<std::optional<AssignmentLookupEntry>> assignmentLookup;
    if (!buildAssignmentLookup(functionAllocation, assignmentLookup, error)) {
      return false;
    }

    for (size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex) {
      const auto &block = function.blocks[blockIndex];
      const auto &plannedBlock = plannedFunction.blocks[blockIndex];
      if (block.instructions.size() != plannedBlock.instructions.size()) {
        error = "spill insertion verifier requires matching instruction counts";
        return false;
      }
      if (block.successorEdges.size() != plannedBlock.successorEdges.size()) {
        error = "spill insertion verifier requires matching successor edge counts";
        return false;
      }

      for (size_t instructionIndex = 0; instructionIndex < block.instructions.size(); ++instructionIndex) {
        if (!verifyInstructionOps(block.instructions[instructionIndex],
                                  plannedBlock.instructions[instructionIndex].beforeInstructionOps,
                                  plannedBlock.instructions[instructionIndex].afterInstructionOps,
                                  assignmentLookup,
                                  error)) {
          error = "spill insertion verifier failed in function " + function.name + " block " + std::to_string(blockIndex) +
                  " instruction " + std::to_string(instructionIndex) + ": " + error;
          return false;
        }
      }

      for (size_t edgeIndex = 0; edgeIndex < block.successorEdges.size(); ++edgeIndex) {
        const auto expectedEdgeOps = computeExpectedEdgeOps(block.successorEdges[edgeIndex], assignmentLookup);
        if (plannedBlock.successorEdges[edgeIndex].successorBlockIndex !=
            block.successorEdges[edgeIndex].successorBlockIndex) {
          error = "spill insertion verifier found mismatched successor block index";
          return false;
        }
        if (plannedBlock.successorEdges[edgeIndex].edgeOps != expectedEdgeOps) {
          error = "spill insertion verifier found mismatched edge spill ops";
          return false;
        }
      }
    }
  }
  return true;
}

} // namespace primec
