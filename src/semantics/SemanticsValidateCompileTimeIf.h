#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec {

void eraseCompileTimeTypeBindings(Program &program);

bool rewriteCompileTimeIfBranches(Program &program,
                                  bool allowDeferred,
                                  std::string &error);

} // namespace primec
