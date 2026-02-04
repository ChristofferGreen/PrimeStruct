#pragma once

#include <string>

#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec {

class IrLowerer {
 public:
  bool lower(const Program &program, const std::string &entryPath, IrModule &out, std::string &error) const;
};

} // namespace primec
