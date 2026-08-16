#pragma once

#include <string>

#include "primec/ast/Ast.h"

namespace primec {

class GlslEmitter {
public:
  bool emitSource(const Program &program, const std::string &entryPath, std::string &out, std::string &error) const;
};

} // namespace primec
