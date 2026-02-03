#pragma once

#include <string>

#include "primec/Ast.h"

namespace primec {

class IrPrinter {
public:
  std::string print(const Program &program) const;
};

} // namespace primec
