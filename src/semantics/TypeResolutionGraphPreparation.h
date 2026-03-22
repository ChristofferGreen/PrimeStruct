#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

bool prepareProgramForTypeResolutionAnalysis(Program &program,
                                             const std::string &entryPath,
                                             const std::vector<std::string> &semanticTransforms,
                                             std::string &error);

} // namespace primec::semantics
