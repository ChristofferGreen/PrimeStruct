#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

namespace primec {

struct IrVirtualRegisterInstruction {
  IrInstruction instruction;
  std::vector<uint32_t> useRegisters;
  std::vector<uint32_t> defRegisters;
};

struct IrVirtualRegisterEdgeMove {
  uint32_t sourceRegister = 0;
  uint32_t destinationRegister = 0;
};

struct IrVirtualRegisterEdge {
  size_t successorBlockIndex = 0;
  std::vector<IrVirtualRegisterEdgeMove> stackMoves;
};

struct IrVirtualRegisterBlock {
  size_t startInstructionIndex = 0;
  size_t endInstructionIndex = 0;
  bool reachable = false;
  std::vector<uint32_t> entryRegisters;
  std::vector<uint32_t> exitRegisters;
  std::vector<IrVirtualRegisterInstruction> instructions;
  std::vector<IrVirtualRegisterEdge> successorEdges;
};

struct IrVirtualRegisterFunction {
  std::string name;
  IrExecutionMetadata metadata;
  std::vector<IrLocalDebugSlot> localDebugSlots;
  std::vector<IrVirtualRegisterBlock> blocks;
  uint32_t nextVirtualRegister = 0;
};

struct IrVirtualRegisterModule {
  std::vector<IrVirtualRegisterFunction> functions;
  int32_t entryIndex = -1;
  std::vector<std::string> stringTable;
  std::vector<IrStructLayout> structLayouts;
  std::vector<IrInstructionSourceMapEntry> instructionSourceMap;
};

bool lowerIrModuleToBlockVirtualRegisters(const IrModule &module, IrVirtualRegisterModule &out, std::string &error);

bool liftBlockVirtualRegistersToIrModule(const IrVirtualRegisterModule &virtualModule, IrModule &out, std::string &error);

} // namespace primec
