#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

namespace primec {

struct NativeEmitterFunctionInstrumentation {
  uint64_t functionIndex = 0;
  std::string functionName;
  uint64_t instructionTotal = 0;
  uint64_t valueStackPushCount = 0;
  uint64_t valueStackPopCount = 0;
  uint64_t spillCount = 0;
  uint64_t reloadCount = 0;
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

} // namespace primec
