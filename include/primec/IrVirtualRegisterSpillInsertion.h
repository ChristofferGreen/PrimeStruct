#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/IrVirtualRegisterAllocator.h"
#include "primec/IrVirtualRegisterLowering.h"

namespace primec {

enum class IrVirtualRegisterSpillOpKind : uint8_t {
  ReloadFromSpill = 0,
  SpillToSlot = 1,
};

struct IrVirtualRegisterSpillOp {
  IrVirtualRegisterSpillOpKind kind = IrVirtualRegisterSpillOpKind::ReloadFromSpill;
  uint32_t virtualRegister = 0;
  uint32_t spillSlot = 0;

  bool operator==(const IrVirtualRegisterSpillOp &other) const = default;
};

struct IrVirtualRegisterSpillPlannedInstruction {
  IrVirtualRegisterInstruction instruction;
  std::vector<IrVirtualRegisterSpillOp> beforeInstructionOps;
  std::vector<IrVirtualRegisterSpillOp> afterInstructionOps;
};

struct IrVirtualRegisterSpillPlannedEdge {
  size_t successorBlockIndex = 0;
  std::vector<IrVirtualRegisterSpillOp> edgeOps;
};

struct IrVirtualRegisterSpillPlannedBlock {
  size_t startInstructionIndex = 0;
  size_t endInstructionIndex = 0;
  bool reachable = false;
  std::vector<IrVirtualRegisterSpillPlannedInstruction> instructions;
  std::vector<IrVirtualRegisterSpillPlannedEdge> successorEdges;
};

struct IrVirtualRegisterSpillPlannedFunction {
  std::string functionName;
  uint32_t spillSlotCount = 0;
  std::vector<IrVirtualRegisterSpillPlannedBlock> blocks;
};

struct IrVirtualRegisterSpillPlan {
  std::vector<IrVirtualRegisterSpillPlannedFunction> functions;
};

bool insertIrVirtualRegisterSpills(const IrVirtualRegisterModule &module,
                                   const IrLinearScanModuleAllocation &allocation,
                                   IrVirtualRegisterSpillPlan &out,
                                   std::string &error);

bool verifyIrVirtualRegisterSpillPlan(const IrVirtualRegisterModule &module,
                                      const IrLinearScanModuleAllocation &allocation,
                                      const IrVirtualRegisterSpillPlan &plan,
                                      std::string &error);

} // namespace primec
