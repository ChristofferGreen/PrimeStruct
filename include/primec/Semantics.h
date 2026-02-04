#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec {

class Semantics {
public:
  bool validate(const Program &program,
                const std::string &entryPath,
                std::string &error,
                const std::vector<std::string> &defaultEffects) const;
};

} // namespace primec
