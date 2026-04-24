#pragma once

#include "primec/Ast.h"

#include <cstdint>

namespace primec {

struct ProgramHeapEstimateStats {
  uint64_t definitions = 0;
  uint64_t executions = 0;
  uint64_t exprs = 0;
  uint64_t transforms = 0;
  uint64_t strings = 0;
  uint64_t dynamicBytes = 0;
};

ProgramHeapEstimateStats estimateProgramHeap(const Program &program);

void releaseLoweredAstBodies(Program &program);

} // namespace primec
