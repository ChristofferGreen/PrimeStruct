#pragma once

#include "SemanticPublicationSurface.h"
#include "primec/Semantics.h"

#include <string>

namespace primec {
namespace semantics {

SemanticProgram buildSemanticProgramFromPublicationSurface(
    const Program &program,
    const std::string &entryPath,
    SemanticPublicationSurface publicationSurface,
    const SemanticProductBuildConfig *buildConfig);

} // namespace semantics
} // namespace primec
