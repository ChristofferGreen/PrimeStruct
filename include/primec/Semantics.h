#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec {

class Semantics {
public:
  bool validate(Program &program,
                const std::string &entryPath,
                std::string &error,
                const std::vector<std::string> &defaultEffects,
                const std::vector<std::string> &semanticTransforms = {}) const;
};

} // namespace primec
