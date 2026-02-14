#pragma once

#include <string>

#include "primec/Ast.h"

namespace primec {

class GlslEmitter {
public:
  bool emitSource(const Program &program, const std::string &entryPath, std::string &out, std::string &error) const;
};

} // namespace primec
