#pragma once

#include "primec/Semantics.h"

#include "SemanticPublicationSurface.h"
#include "SemanticsValidationBenchmarkOrchestration.h"

#include <string>

namespace primec::semantics {

void publishSemanticProgramAfterValidation(
    const Program &program,
    const std::string &entryPath,
    SemanticPublicationSurface publicationSurface,
    const SemanticProductBuildConfig *buildConfig,
    const SemanticValidationBenchmarkRuntime &benchmarkRuntime,
    SemanticProgram &semanticProgramOut);

} // namespace primec::semantics
