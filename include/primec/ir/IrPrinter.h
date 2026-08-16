#pragma once

#include <string>

#include "primec/ast/Ast.h"

namespace primec {

class IrPrinter {
public:
  std::string print(const Program &program) const;
};

} // namespace primec
