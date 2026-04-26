#pragma once

#include "primec/Semantics.h"

#include "SemanticsValidationBenchmarkOrchestration.h"

#include <string>

namespace primec::semantics {

class SemanticsValidator;

void publishSemanticProgramAfterValidation(
    const Program &program,
    const std::string &entryPath,
    SemanticsValidator &validator,
    const SemanticProductBuildConfig *buildConfig,
    const SemanticValidationBenchmarkRuntime &benchmarkRuntime,
    SemanticProgram &semanticProgramOut);

} // namespace primec::semantics
