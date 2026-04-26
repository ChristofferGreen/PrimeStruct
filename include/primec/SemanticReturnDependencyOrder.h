#pragma once

#include "primec/Ast.h"

#include <string>
#include <vector>

namespace primec::semantics {

struct ReturnDependencyComponent {
  std::vector<std::string> definitionPaths;
  bool hasCycle = false;
};

std::vector<ReturnDependencyComponent> buildReturnDependencyOrder(const Program &program);

} // namespace primec::semantics
