#pragma once

#include <string>

#include "primec/Ast.h"

namespace primec {

class AstPrinter {
public:
  std::string print(const Program &program) const;
};

} // namespace primec
