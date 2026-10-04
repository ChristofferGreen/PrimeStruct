#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

struct NativeEmitterFunctionInstrumentation {
  uint64_t functionIndex = 0;
  std::string functionName;
  uint64_t instructionTotal = 0;
  uint64_t valueStackPushCount = 0;
  uint64_t valueStackPopCount = 0;
  uint64_t spillCount = 0;
  uint64_t reloadCount = 0;
  // x86_64 register allocation (NativeEmitterOptions::registerAllocation): whether this function
  // was emitted from a register plan, how many spill slots the plan needed, and, when allocation
  // was enabled but the function fell back to the template emitter, why.
  bool registerAllocated = false;
  uint64_t registerAllocationSpillSlots = 0;
  std::string registerAllocationFallback;
};

struct NativeEmitterInstrumentation {
  std::vector<NativeEmitterFunctionInstrumentation> perFunction;
  uint64_t totalInstructionCount = 0;
  uint64_t totalValueStackPushCount = 0;
  uint64_t totalValueStackPopCount = 0;
  uint64_t totalSpillCount = 0;
  uint64_t totalReloadCount = 0;
};

struct NativeEmitterOptimizationInstrumentation {
  bool applied = false;
  uint64_t instructionTotalBefore = 0;
  uint64_t instructionTotalAfter = 0;
  uint64_t valueStackPushCountBefore = 0;
  uint64_t valueStackPushCountAfter = 0;
  uint64_t valueStackPopCountBefore = 0;
  uint64_t valueStackPopCountAfter = 0;
  uint64_t spillCountBefore = 0;
  uint64_t spillCountAfter = 0;
  uint64_t reloadCountBefore = 0;
  uint64_t reloadCountAfter = 0;
};

struct NativeEmitterOptions {
  bool enableRegisterCache = true;
  // x86_64 only: keep the hottest locals that no memory access can reach in
  // machine registers (docs/OptimizingBackendsPlan.md, Phase 3). Off unless the
  // backend turns it on for -O1 and above.
  bool promoteLocals = false;
  // x86_64 only: defer pushes of constants and register-resident locals and
  // compute on registers and immediates, writing operands to the memory stack
  // only where they must outlive an instruction sequence.
  bool deferOperands = false;
  // x86_64 only: allocate registers for every value of a function, locals and operands alike
  // (src/native_emitter/NativeEmitterRegAlloc.h), instead of promoting locals and deferring
  // operands. The PRIMESTRUCT_NATIVE_REGALLOC environment variable forces it on (1) or off (0).
  bool registerAllocation = false;
};

class NativeEmitter {
 public:
  bool emitExecutable(const IrModule &module, const std::string &outputPath, std::string &error) const;
  bool emitExecutable(const IrModule &module,
                      const std::string &outputPath,
                      std::string &error,
                      NativeEmitterInstrumentation *instrumentation) const;
  bool emitExecutable(const IrModule &module,
                      const std::string &outputPath,
                      std::string &error,
                      NativeEmitterInstrumentation *instrumentation,
                      const NativeEmitterOptions &options) const;
};

std::string formatNativeEmitterDebugDump(
    const NativeEmitterInstrumentation &instrumentation,
    const NativeEmitterOptimizationInstrumentation &optimization = NativeEmitterOptimizationInstrumentation{});

// Text for --opt-report on the native backend: one line per function saying whether it was
// register-allocated (with its spill slot count) or emitted by the template path (with the
// reason when allocation was enabled), in function index order.
std::string
formatNativeRegisterAllocationReport(const NativeEmitterInstrumentation &instrumentation);

} // namespace primec
