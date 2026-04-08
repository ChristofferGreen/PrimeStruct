#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

bool prepareProgramForTypeResolutionAnalysis(Program &program,
                                             const std::string &entryPath,
                                             const std::vector<std::string> &semanticTransforms,
                                             std::string &error,
                                             uint64_t *explicitTemplateArgFactHitCountOut = nullptr,
                                             uint64_t *implicitTemplateArgFactHitCountOut = nullptr);

} // namespace primec::semantics
