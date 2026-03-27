#pragma once

#include "primec/Ir.h"

#include <sstream>
#include <string>
#include <vector>

namespace primec::ir_to_glsl_internal {

bool emitFunction(const IrFunction &function,
                  size_t functionIndex,
                  size_t functionCount,
                  const std::vector<std::string> &stringTable,
                  std::ostringstream &out,
                  std::string &error);

} // namespace primec::ir_to_glsl_internal
