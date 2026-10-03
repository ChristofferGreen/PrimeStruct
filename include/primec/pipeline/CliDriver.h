#pragma once

#include "primec/pipeline/CompilePipeline.h"
#include "primec/backend/IrBackends.h"
#include "primec/ir/IrPreparation.h"
#include "primec/support/Options.h"
#include "primec/support/TransformRegistry.h"

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

// True for `--dump-stage=ir-lowered` and `--dump-stage=ir-optimized`, which
// print the lowered IrModule (see formatIrModule) instead of an AST dump. The
// compile pipeline runs normally for these stages; the CLI front ends call
// runIrModuleDump afterwards instead of emitting a backend's output.
bool isIrModuleDumpStage(const Options &options);

// Lowers and validates like a backend would (the optimization phase runs only
// for ir-optimized) and prints the listing to `out`. Returns the process exit
// code; failures are reported to `err` in the usual CLI format.
int runIrModuleDump(std::ostream &out,
                    std::ostream &err,
                    Program &program,
                    const SemanticProgram *semanticProgram,
                    const ExpandedSource *expandedSource,
                    const Options &options);

} // namespace primec
