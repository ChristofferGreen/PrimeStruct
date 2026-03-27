#pragma once

#include "primec/Ir.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace primec {

struct EmitContext {
  size_t stringCount = 0;
  size_t functionCount = 0;
  std::vector<size_t> stringLengths;
};

bool emitInstruction(const IrInstruction &instruction,
                     size_t index,
                     size_t nextIndex,
                     size_t instructionCount,
                     size_t localCount,
                     const EmitContext &context,
                     std::ostringstream &out,
                     std::string &error);

bool emitPrintAndFileInstruction(const IrInstruction &instruction,
                                 size_t index,
                                 size_t nextIndex,
                                 size_t localCount,
                                 const EmitContext &context,
                                 std::ostringstream &out,
                                 std::string &error);

} // namespace primec
