#include "primec/CliDriver.h"

#include "primec/Diagnostics.h"

#include <algorithm>

namespace primec {

std::string transformAvailability(const TransformInfo &info) {
  std::string availability;
  if (info.availableInPrimec) {
    availability = "primec";
  }
  if (info.availableInPrimevm) {
    if (!availability.empty()) {
      availability += ",";
    }
    availability += "primevm";
  }
  if (availability.empty()) {
    return "none";
  }
  return availability;
}

void printTransformList(std::ostream &out) {
  out << "name\tphase\taliases\tavailability\n";
  for (const auto &transform : listTransforms()) {
    out << transform.name << '\t' << transformPhaseName(transform.phase) << '\t' << transform.aliases << '\t'
        << transformAvailability(transform) << '\n';
  }
}

int emitCliFailure(std::ostream &err, const Options &options, const CliFailure &failure) {
  std::vector<DiagnosticRecord> diagnostics;
  if (failure.diagnosticInfo != nullptr && !failure.diagnosticInfo->records.empty()) {
    diagnostics.reserve(failure.diagnosticInfo->records.size());
    for (const auto &recordInfo : failure.diagnosticInfo->records) {
      std::string diagnosticMessage =
          recordInfo.normalizedMessage.empty() ? failure.message : recordInfo.normalizedMessage;
      const DiagnosticSpan *primarySpan = nullptr;
      if (recordInfo.hasPrimarySpan) {
        primarySpan = &recordInfo.primarySpan;
      }
      diagnostics.push_back(makeDiagnosticRecord(
          failure.code, diagnosticMessage, options.inputPath, failure.notes, primarySpan, recordInfo.relatedSpans));
    }
  } else {
    std::string diagnosticMessage = failure.message;
    DiagnosticSpan primarySpanStorage;
    const DiagnosticSpan *primarySpan = nullptr;
    std::vector<DiagnosticRelatedSpan> relatedSpans;
    if (failure.diagnosticInfo != nullptr) {
      if (!failure.diagnosticInfo->normalizedMessage.empty()) {
        diagnosticMessage = failure.diagnosticInfo->normalizedMessage;
      }
      if (failure.diagnosticInfo->hasPrimarySpan) {
        primarySpanStorage = failure.diagnosticInfo->primarySpan;
        primarySpan = &primarySpanStorage;
      }
      relatedSpans = failure.diagnosticInfo->relatedSpans;
    }
    diagnostics.push_back(
        makeDiagnosticRecord(failure.code, diagnosticMessage, options.inputPath, failure.notes, primarySpan, relatedSpans));
  }

  if (options.emitDiagnostics) {
    err << encodeDiagnosticsJson(diagnostics) << "\n";
    return failure.exitCode;
  }

  const bool hasLocation = std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto &record) {
    return record.primarySpan.line > 0 && record.primarySpan.column > 0;
  });
  if (hasLocation) {
    err << formatDiagnosticsText(diagnostics, failure.plainPrefix, failure.sourceText);
    return failure.exitCode;
  }

  err << failure.plainPrefix << failure.message << "\n";
  return failure.exitCode;
}

CliFailure describeCompilePipelineFailure(const CompilePipelineErrorStage errorStage,
                                          const std::string &message,
                                          const CompilePipelineOutput &output,
                                          const CompilePipelineDiagnosticInfo &diagnosticInfo) {
  CliFailure failure;
  failure.message = message;
  switch (errorStage) {
    case CompilePipelineErrorStage::Import:
      failure.code = DiagnosticCode::ImportError;
      failure.plainPrefix = "Import error: ";
      failure.notes = {"stage: import"};
      break;
    case CompilePipelineErrorStage::Transform:
      failure.code = DiagnosticCode::TransformError;
      failure.plainPrefix = "Transform error: ";
      failure.notes = {"stage: transform"};
      break;
    case CompilePipelineErrorStage::Parse:
      failure.code = DiagnosticCode::ParseError;
      failure.plainPrefix = "Parse error: ";
      failure.notes = {"stage: parse"};
      failure.diagnosticInfo = &diagnosticInfo;
      failure.sourceText = &output.filteredSource;
      break;
    case CompilePipelineErrorStage::UnsupportedDumpStage:
      failure.code = DiagnosticCode::UnsupportedDumpStage;
      failure.plainPrefix = "Unsupported dump stage: ";
      failure.notes = {"stage: dump-stage"};
      break;
    case CompilePipelineErrorStage::Semantic:
      failure.code = DiagnosticCode::SemanticError;
      failure.plainPrefix = "Semantic error: ";
      failure.notes = {"stage: semantic"};
      failure.diagnosticInfo = &diagnosticInfo;
      failure.sourceText = &output.filteredSource;
      break;
    case CompilePipelineErrorStage::None:
    default:
      failure.code = DiagnosticCode::EmitError;
      failure.plainPrefix = "Compile pipeline error: ";
      failure.notes = {"stage: compile-pipeline"};
      break;
  }
  return failure;
}

std::vector<std::string> makeIrBackendNotes(const IrBackendDiagnostics &diagnostics, std::string_view stage) {
  std::vector<std::string> notes;
  notes.reserve(stage.empty() ? 1 : 2);
  notes.push_back("backend: " + std::string(diagnostics.backendTag));
  if (!stage.empty()) {
    notes.push_back("stage: " + std::string(stage));
  }
  return notes;
}

CliFailure describeIrPreparationFailure(const IrPreparationFailure &failure, const IrBackend &backend, int exitCode) {
  CliFailure cliFailure;
  cliFailure.exitCode = exitCode;
  cliFailure.message = failure.message;

  const IrBackendDiagnostics &diagnostics = backend.diagnostics();
  switch (failure.stage) {
    case IrPreparationFailureStage::Validation:
      cliFailure.code = diagnostics.validationDiagnosticCode;
      cliFailure.plainPrefix = diagnostics.validationErrorPrefix;
      cliFailure.notes = makeIrBackendNotes(diagnostics, "ir-validate");
      break;
    case IrPreparationFailureStage::Inlining:
      cliFailure.code = diagnostics.inliningDiagnosticCode;
      cliFailure.plainPrefix = diagnostics.inliningErrorPrefix;
      cliFailure.notes = makeIrBackendNotes(diagnostics, "ir-inline");
      break;
    case IrPreparationFailureStage::Lowering:
    case IrPreparationFailureStage::None:
    default:
      cliFailure.code = diagnostics.loweringDiagnosticCode;
      cliFailure.plainPrefix = diagnostics.loweringErrorPrefix;
      cliFailure.notes = makeIrBackendNotes(diagnostics);
      backend.normalizeLoweringError(cliFailure.message);
      break;
  }

  return cliFailure;
}

} // namespace primec
