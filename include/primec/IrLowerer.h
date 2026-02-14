#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec {

class IrLowerer {
 public:
  bool lower(const Program &program,
             const std::string &entryPath,
             const std::vector<std::string> &defaultEffects,
             IrModule &out,
             std::string &error) const;
};

} // namespace primec
