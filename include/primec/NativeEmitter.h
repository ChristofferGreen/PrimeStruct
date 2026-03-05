#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

namespace primec {

struct NativeEmitterFunctionInstrumentation {
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

class NativeEmitter {
 public:
  bool emitExecutable(const IrModule &module, const std::string &outputPath, std::string &error) const;
  bool emitExecutable(const IrModule &module,
                      const std::string &outputPath,
                      std::string &error,
                      NativeEmitterInstrumentation *instrumentation) const;
};

} // namespace primec
