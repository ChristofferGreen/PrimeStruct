#pragma once

#include <string>
#include <vector>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

bool applySemanticTransforms(Program &program,
                             const std::vector<std::string> &semanticTransforms,
                             std::string &error);

} // namespace primec::semantics
