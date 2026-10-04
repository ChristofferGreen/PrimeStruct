#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

struct IrVirtualRegisterInstruction {
  IrInstruction instruction;
  std::vector<uint32_t> useRegisters;
  std::vector<uint32_t> defRegisters;
  // Promoted-locals mode only (IrVirtualRegisterLoweringOptions::promoteLocals): a LoadLocal of
  // a promoted slot reads the register holding the local's current value, and a StoreLocal of one
  // defines a new register for it. The stack operands above are unchanged.
  std::optional<uint32_t> localUseRegister;
  std::optional<uint32_t> localDefRegister;
};

// A promoted local's value at a block boundary.
struct IrVirtualRegisterLocalValue {
  uint32_t local = 0;
  uint32_t reg = 0;

  bool operator==(const IrVirtualRegisterLocalValue &other) const = default;
};

struct IrVirtualRegisterLocalMove {
  uint32_t local = 0;
  uint32_t sourceRegister = 0;
  uint32_t destinationRegister = 0;

  bool operator==(const IrVirtualRegisterLocalMove &other) const = default;
};

struct IrVirtualRegisterEdgeMove {
  uint32_t sourceRegister = 0;
  uint32_t destinationRegister = 0;

  bool operator==(const IrVirtualRegisterEdgeMove &other) const = default;
};

struct IrVirtualRegisterEdge {
  size_t successorBlockIndex = 0;
  std::vector<IrVirtualRegisterEdgeMove> stackMoves;
  // One move per promoted local live into the successor (promoted-locals mode).
  std::vector<IrVirtualRegisterLocalMove> localMoves;
};

struct IrVirtualRegisterBlock {
  size_t startInstructionIndex = 0;
  size_t endInstructionIndex = 0;
  bool reachable = false;
  std::vector<uint32_t> entryRegisters;
  std::vector<uint32_t> exitRegisters;
  std::vector<IrVirtualRegisterInstruction> instructions;
  std::vector<IrVirtualRegisterEdge> successorEdges;
  // Promoted locals live into / out of the block with the register holding each, ascending by
  // local (promoted-locals mode). A reachable entry block with entryLocals reads a local before
  // any definition; the local-form verifier rejects that.
  std::vector<IrVirtualRegisterLocalValue> entryLocals;
  std::vector<IrVirtualRegisterLocalValue> exitLocals;
};

struct IrVirtualRegisterFunction {
  std::string name;
  IrExecutionMetadata metadata;
  std::vector<IrLocalDebugSlot> localDebugSlots;
  std::vector<IrVirtualRegisterBlock> blocks;
  uint32_t nextVirtualRegister = 0;
  // Locals held in registers in promoted-locals mode: not pinned by IrLocalEscape.h.
  std::vector<uint32_t> promotedLocals;
};

struct IrVirtualRegisterModule {
  std::vector<IrVirtualRegisterFunction> functions;
  int32_t entryIndex = -1;
  std::vector<std::string> stringTable;
  std::vector<IrStructLayout> structLayouts;
  std::vector<IrInstructionSourceMapEntry> instructionSourceMap;
};

struct IrVirtualRegisterLoweringOptions {
  // Give every local that no memory access can reach (see IrLocalEscape.h) virtual registers,
  // with block-boundary values and edge moves like the stack. The x86_64 native register
  // allocator (NativeEmitterRegAlloc.h) consumes this form; the generic allocator, scheduler and
  // spill insertion here expect the default form.
  bool promoteLocals = false;
};

bool lowerIrModuleToBlockVirtualRegisters(const IrModule &module,
                                          IrVirtualRegisterModule &out,
                                          std::string &error,
                                          const IrVirtualRegisterLoweringOptions &options = {});

// One function of `module` (the module supplies call stack effects).
bool lowerIrFunctionToBlockVirtualRegisters(const IrModule &module,
                                            size_t functionIndex,
                                            IrVirtualRegisterFunction &out,
                                            std::string &error,
                                            const IrVirtualRegisterLoweringOptions &options = {});

bool liftBlockVirtualRegistersToIrModule(const IrVirtualRegisterModule &virtualModule, IrModule &out, std::string &error);

} // namespace primec
