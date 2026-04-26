#include "primec/CliDriver.h"

#include "primec/Diagnostics.h"

#include <algorithm>
#include <ostream>

namespace primec {
namespace {

DiagnosticSpan normalizeSpanFile(DiagnosticSpan span, const std::string &inputPath) {
  if (span.file.empty()) {
    span.file = inputPath;
  }
  return span;
}

std::vector<DiagnosticRelatedSpan> normalizeRelatedSpanFiles(std::vector<DiagnosticRelatedSpan> relatedSpans,
                                                             const std::string &inputPath) {
  for (auto &related : relatedSpans) {
    related.span = normalizeSpanFile(std::move(related.span), inputPath);
  }
  return relatedSpans;
}

} // namespace

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
  if (failure.diagnosticInfo.has_value() && !failure.diagnosticInfo->records.empty()) {
    diagnostics.reserve(failure.diagnosticInfo->records.size());
    for (const auto &recordInfo : failure.diagnosticInfo->records) {
      std::string diagnosticMessage = recordInfo.message.empty() ? failure.message : recordInfo.message;
      const DiagnosticSpan *primarySpan = nullptr;
      DiagnosticSpan normalizedPrimarySpan;
      if (recordInfo.hasPrimarySpan) {
        normalizedPrimarySpan = normalizeSpanFile(recordInfo.primarySpan, options.inputPath);
        primarySpan = &normalizedPrimarySpan;
      }
      diagnostics.push_back(makeDiagnosticRecord(
          failure.code,
          diagnosticMessage,
          options.inputPath,
          failure.notes,
          primarySpan,
          normalizeRelatedSpanFiles(recordInfo.relatedSpans, options.inputPath)));
    }
  } else {
    std::string diagnosticMessage = failure.message;
    DiagnosticSpan primarySpanStorage;
    const DiagnosticSpan *primarySpan = nullptr;
    std::vector<DiagnosticRelatedSpan> relatedSpans;
    if (failure.diagnosticInfo.has_value()) {
      if (!failure.diagnosticInfo->message.empty()) {
        diagnosticMessage = failure.diagnosticInfo->message;
      }
      if (failure.diagnosticInfo->hasPrimarySpan) {
        primarySpanStorage = normalizeSpanFile(failure.diagnosticInfo->primarySpan, options.inputPath);
        primarySpan = &primarySpanStorage;
      }
      relatedSpans = normalizeRelatedSpanFiles(failure.diagnosticInfo->relatedSpans, options.inputPath);
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
    const std::string *sourceText = failure.sourceText.has_value() ? &*failure.sourceText : nullptr;
    err << formatDiagnosticsText(diagnostics, failure.plainPrefix, sourceText);
    return failure.exitCode;
  }

  err << failure.plainPrefix << failure.message << "\n";
  return failure.exitCode;
}

namespace {

CliFailure describeCompilePipelineFailureDetails(
    const CompilePipelineFailure &pipelineFailure,
    bool hasSemanticProgram,
    const std::string &filteredSource) {
  CliFailure failure;

  failure.message = pipelineFailure.message;
  switch (pipelineFailure.stage) {
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
      failure.diagnosticInfo = pipelineFailure.diagnosticInfo;
      failure.sourceText = filteredSource;
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
      if (hasSemanticProgram) {
        failure.notes.push_back("semantic-product: available");
      }
      failure.diagnosticInfo = pipelineFailure.diagnosticInfo;
      failure.sourceText = filteredSource;
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

} // namespace

CliFailure describeCompilePipelineFailure(const CompilePipelineOutput &output) {
  if (!output.hasFailure) {
    CliFailure failure;
    failure.code = DiagnosticCode::EmitError;
    failure.plainPrefix = "Compile pipeline error: ";
    failure.notes = {"stage: compile-pipeline"};
    return failure;
  }

  return describeCompilePipelineFailureDetails(
      output.failure, output.hasSemanticProgram, output.filteredSource);
}

CliFailure describeCompilePipelineFailure(const CompilePipelineFailureResult &output) {
  return describeCompilePipelineFailureDetails(
      output.failure, output.hasSemanticProgram, output.filteredSource);
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

  cliFailure.diagnosticInfo = failure.diagnosticInfo;

  return cliFailure;
}

CliFailure describeIrPreparationFailure(const IrPreparationFailure &failure,
                                        const IrBackendDiagnostics &diagnostics,
                                        IrLoweringErrorNormalizer normalizeLoweringError,
                                        int exitCode) {
  CliFailure cliFailure;
  cliFailure.exitCode = exitCode;
  cliFailure.message = failure.message;

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
      if (normalizeLoweringError != nullptr) {
        normalizeLoweringError(cliFailure.message);
      }
      break;
  }

  cliFailure.diagnosticInfo = failure.diagnosticInfo;

  return cliFailure;
}

} // namespace primec
