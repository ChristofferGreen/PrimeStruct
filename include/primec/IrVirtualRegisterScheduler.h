#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/IrVirtualRegisterAllocator.h"
#include "primec/IrVirtualRegisterLowering.h"

namespace primec {

struct IrVirtualRegisterScheduledInstruction {
  IrVirtualRegisterInstruction instruction;
  size_t originalInstructionIndex = 0;
  uint32_t latencyScore = 0;
  std::vector<size_t> dependencyInstructionIndices;
};

struct IrVirtualRegisterScheduledBlock {
  size_t startInstructionIndex = 0;
  size_t endInstructionIndex = 0;
  bool reachable = false;
  std::vector<IrVirtualRegisterScheduledInstruction> instructions;
  std::vector<IrVirtualRegisterEdge> successorEdges;
};

struct IrVirtualRegisterScheduledFunction {
  std::string functionName;
  std::vector<IrVirtualRegisterScheduledBlock> blocks;
};

struct IrVirtualRegisterScheduledModule {
  std::vector<IrVirtualRegisterScheduledFunction> functions;
};

bool scheduleIrVirtualRegisters(const IrVirtualRegisterModule &module,
                                const IrLinearScanModuleAllocation &allocation,
                                IrVirtualRegisterScheduledModule &out,
                                std::string &error);

} // namespace primec
