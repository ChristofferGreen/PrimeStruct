#pragma once

#include "primec/CompilePipeline.h"
#include "primec/IrBackends.h"
#include "primec/IrPreparation.h"
#include "primec/Options.h"
#include "primec/TransformRegistry.h"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace primec {

struct CliFailure {
  DiagnosticCode code = DiagnosticCode::EmitError;
  std::string plainPrefix;
  std::string message;
  int exitCode = 2;
  std::vector<std::string> notes;
  std::optional<DiagnosticSinkReport> diagnosticInfo;
  std::optional<std::string> sourceText;
};

using IrLoweringErrorNormalizer = void (*)(std::string &error);

std::string transformAvailability(const TransformInfo &info);
void printTransformList(std::ostream &out);

int emitCliFailure(std::ostream &err, const Options &options, const CliFailure &failure);

CliFailure describeCompilePipelineFailure(const CompilePipelineOutput &output);

CliFailure describeCompilePipelineFailure(const CompilePipelineFailureResult &output);

CliFailure describeIrPreparationFailure(const IrPreparationFailure &failure,
                                        const IrBackendDiagnostics &diagnostics,
                                        IrLoweringErrorNormalizer normalizeLoweringError = nullptr,
                                        int exitCode = 2);

CliFailure describeIrPreparationFailure(const IrPreparationFailure &failure,
                                        const IrBackend &backend,
                                        int exitCode = 2);

std::vector<std::string> makeIrBackendNotes(const IrBackendDiagnostics &diagnostics, std::string_view stage = {});

} // namespace primec
