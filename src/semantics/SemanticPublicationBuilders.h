#pragma once

#include "primec/Semantics.h"

#include "SemanticsValidator.h"

#include <string>

namespace primec {
namespace semantics {

SemanticProgram buildSemanticProgramFromPublicationSurface(
    const Program &program,
    const std::string &entryPath,
    SemanticsValidator::SemanticPublicationSurface publicationSurface,
    const SemanticProductBuildConfig *buildConfig);

} // namespace semantics
} // namespace primec
