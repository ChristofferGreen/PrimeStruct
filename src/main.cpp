#include "primec/CompilePipeline.h"
#include "primec/Diagnostics.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrPreparation.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "primec/TransformRegistry.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {
bool hasPathPrefix(const std::filesystem::path &path, const std::filesystem::path &prefix) {
  auto pathIter = path.begin();
  auto prefixIter = prefix.begin();
  for (; prefixIter != prefix.end(); ++prefixIter, ++pathIter) {
    if (pathIter == path.end() || *pathIter != *prefixIter) {
      return false;
    }
  }
  return true;
}

std::filesystem::path resolveOutputPath(const primec::Options &options) {
  std::filesystem::path output(options.outputPath);
  if (output.is_absolute()) {
    return output;
  }
  if (options.outDir.empty() || options.outDir == ".") {
    return output;
  }
  std::filesystem::path outDir(options.outDir);
  if (hasPathPrefix(output.lexically_normal(), outDir.lexically_normal())) {
    return output;
  }
  return outDir / output;
}

bool ensureOutputDirectory(const std::filesystem::path &outputPath, std::string &error) {
  std::filesystem::path parent = outputPath.parent_path();
  if (parent.empty()) {
    return true;
  }
  std::error_code ec;
  std::filesystem::create_directories(parent, ec);
  if (ec) {
    error = "failed to create output directory: " + parent.string();
    return false;
  }
  return true;
}

std::string transformAvailability(const primec::TransformInfo &info) {
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
  for (const auto &transform : primec::listTransforms()) {
    out << transform.name << '\t' << primec::transformPhaseName(transform.phase) << '\t' << transform.aliases << '\t'
        << transformAvailability(transform) << '\n';
  }
}

std::vector<std::string> irBackendNotes(const primec::IrBackendDiagnostics &diagnostics,
                                        std::string_view stage = {}) {
  std::vector<std::string> notes;
  notes.reserve(stage.empty() ? 1 : 2);
  notes.push_back("backend: " + std::string(diagnostics.backendTag));
  if (!stage.empty()) {
    notes.push_back("stage: " + std::string(stage));
  }
  return notes;
}

enum class IrBackendRunFailureStage {
  None,
  Lowering,
  Validation,
  Inlining,
  Emit,
  OutputWrite,
};

struct IrBackendRunFailure {
  IrBackendRunFailureStage stage = IrBackendRunFailureStage::None;
  std::string message;
};

bool runIrBackend(const primec::IrBackend &backend,
                  primec::Program &program,
                  const primec::Options &options,
                  primec::IrBackendEmitResult &result,
                  IrBackendRunFailure &failure) {
  failure = {};
  const primec::IrBackendDiagnostics &diagnostics = backend.diagnostics();

  const primec::IrValidationTarget validationTarget = backend.validationTarget(options);
  primec::IrModule ir;
  primec::IrPreparationFailure prepFailure;
  if (!primec::prepareIrModule(program, options, validationTarget, ir, prepFailure)) {
    switch (prepFailure.stage) {
      case primec::IrPreparationFailureStage::Lowering:
        backend.normalizeLoweringError(prepFailure.message);
        failure.stage = IrBackendRunFailureStage::Lowering;
        break;
      case primec::IrPreparationFailureStage::Validation:
        failure.stage = IrBackendRunFailureStage::Validation;
        break;
      case primec::IrPreparationFailureStage::Inlining:
        failure.stage = IrBackendRunFailureStage::Inlining;
        break;
      default:
        failure.stage = IrBackendRunFailureStage::Lowering;
        break;
    }
    failure.message = std::move(prepFailure.message);
    return false;
  }

  std::string error;
  primec::IrBackendEmitOptions emitOptions;
  emitOptions.outputPath = options.outputPath;
  emitOptions.inputPath = options.inputPath;
  emitOptions.programArgs = options.programArgs;
  if (!backend.emit(ir, emitOptions, result, error)) {
    const std::string_view backendTag = diagnostics.backendTag;
    const bool outputWriteFailure =
        (backendTag == "ir" || backendTag == "wasm" || backendTag == "cpp-ir" || backendTag == "glsl-ir") &&
        error == options.outputPath;
    failure.stage = outputWriteFailure ? IrBackendRunFailureStage::OutputWrite : IrBackendRunFailureStage::Emit;
    failure.message = std::move(error);
    return false;
  }

  return true;
}

int emitFailure(const primec::Options &options,
                primec::DiagnosticCode code,
                const std::string &plainPrefix,
                const std::string &message,
                int exitCode,
                const std::vector<std::string> &notes = {},
                const primec::CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr,
                const std::string *sourceText = nullptr) {
  std::vector<primec::DiagnosticRecord> diagnostics;
  if (diagnosticInfo != nullptr && !diagnosticInfo->records.empty()) {
    diagnostics.reserve(diagnosticInfo->records.size());
    for (const auto &recordInfo : diagnosticInfo->records) {
      std::string diagnosticMessage = recordInfo.normalizedMessage.empty() ? message : recordInfo.normalizedMessage;
      const primec::DiagnosticSpan *primarySpan = nullptr;
      if (recordInfo.hasPrimarySpan) {
        primarySpan = &recordInfo.primarySpan;
      }
      diagnostics.push_back(primec::makeDiagnosticRecord(
          code, diagnosticMessage, options.inputPath, notes, primarySpan, recordInfo.relatedSpans));
    }
  } else {
    std::string diagnosticMessage = message;
    primec::DiagnosticSpan primarySpanStorage;
    const primec::DiagnosticSpan *primarySpan = nullptr;
    std::vector<primec::DiagnosticRelatedSpan> relatedSpans;
    if (diagnosticInfo != nullptr) {
      if (!diagnosticInfo->normalizedMessage.empty()) {
        diagnosticMessage = diagnosticInfo->normalizedMessage;
      }
      if (diagnosticInfo->hasPrimarySpan) {
        primarySpanStorage = diagnosticInfo->primarySpan;
        primarySpan = &primarySpanStorage;
      }
      relatedSpans = diagnosticInfo->relatedSpans;
    }
    diagnostics.push_back(
        primec::makeDiagnosticRecord(code, diagnosticMessage, options.inputPath, notes, primarySpan, relatedSpans));
  }

  if (options.emitDiagnostics) {
    std::cerr << primec::encodeDiagnosticsJson(diagnostics) << "\n";
    return exitCode;
  }

  const bool hasLocation = std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto &record) {
    return record.primarySpan.line > 0 && record.primarySpan.column > 0;
  });
  if (hasLocation) {
    std::cerr << primec::formatDiagnosticsText(diagnostics, plainPrefix, sourceText);
    return exitCode;
  }

  std::cerr << plainPrefix << message << "\n";
  return exitCode;
}

} // namespace

int main(int argc, char **argv) {
  primec::Options options;
  std::string argError;
  if (!primec::parseOptions(argc, argv, primec::OptionsParserMode::Primec, options, argError)) {
    if (options.emitDiagnostics) {
      if (argError.empty()) {
        argError = "invalid arguments";
      }
      const primec::DiagnosticRecord diagnostic =
          primec::makeDiagnosticRecord(primec::DiagnosticCode::ArgumentError, argError, options.inputPath);
      std::cerr << primec::encodeDiagnosticsJson({diagnostic}) << "\n";
    } else {
      if (!argError.empty()) {
        std::cerr << "Argument error: " << argError << "\n";
      }
      std::cerr << "Usage: primec [--emit=" << primec::primecEmitKindsUsage() << "] <input.prime> [-o <output>] "
                << "[--entry /path] [--import-path <dir>] [-I <dir>] "
                << "[--wasm-profile wasi|browser] "
                << "[--text-transforms <list>] [--text-transform-rules <rules>] "
                << "[--semantic-transform-rules <rules>] [--semantic-transforms <list>] "
                << "[--transform-list <list>] [--no-text-transforms] [--no-semantic-transforms] "
                << "[--no-transforms] [--out-dir <dir>] [--list-transforms] [--emit-diagnostics] "
                << "[--collect-diagnostics] "
                << "[--default-effects <list>] [--ir-inline] "
                << "[--dump-stage pre_ast|ast|ast-semantic|ir] [-- <program args...>]\n";
    }
    return 2;
  }
  if (options.listTransforms) {
    printTransformList(std::cout);
    return 0;
  }
  std::string error;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput pipelineOutput;
  primec::CompilePipelineDiagnosticInfo pipelineDiagnosticInfo;
  primec::CompilePipelineErrorStage pipelineError = primec::CompilePipelineErrorStage::None;
  if (!primec::runCompilePipeline(options, pipelineOutput, pipelineError, error, &pipelineDiagnosticInfo)) {
    switch (pipelineError) {
      case primec::CompilePipelineErrorStage::Import:
        return emitFailure(options,
                           primec::DiagnosticCode::ImportError,
                           "Import error: ",
                           error,
                           2,
                           {"stage: import"});
      case primec::CompilePipelineErrorStage::Transform:
        return emitFailure(options,
                           primec::DiagnosticCode::TransformError,
                           "Transform error: ",
                           error,
                           2,
                           {"stage: transform"});
      case primec::CompilePipelineErrorStage::Parse:
        return emitFailure(options,
                           primec::DiagnosticCode::ParseError,
                           "Parse error: ",
                           error,
                           2,
                           {"stage: parse"},
                           &pipelineDiagnosticInfo,
                           &pipelineOutput.filteredSource);
      case primec::CompilePipelineErrorStage::UnsupportedDumpStage:
        return emitFailure(options,
                           primec::DiagnosticCode::UnsupportedDumpStage,
                           "Unsupported dump stage: ",
                           error,
                           2,
                           {"stage: dump-stage"});
      case primec::CompilePipelineErrorStage::Semantic:
        return emitFailure(options,
                           primec::DiagnosticCode::SemanticError,
                           "Semantic error: ",
                           error,
                           2,
                           {"stage: semantic"},
                           &pipelineDiagnosticInfo,
                           &pipelineOutput.filteredSource);
      default:
        return emitFailure(options,
                           primec::DiagnosticCode::EmitError,
                           "Compile pipeline error: ",
                           error,
                           2,
                           {"stage: compile-pipeline"});
    }
  }
  if (pipelineOutput.hasDumpOutput) {
    std::cout << pipelineOutput.dumpOutput;
    return 0;
  }

  primec::Program &program = pipelineOutput.program;

  const std::string_view irBackendKind = primec::resolveIrBackendEmitKind(options.emitKind);
  const primec::IrBackend *irBackend = primec::findIrBackend(irBackendKind);

  if (irBackend != nullptr && irBackend->requiresOutputPath() && !options.outputPath.empty()) {
    std::filesystem::path resolved = resolveOutputPath(options);
    options.outputPath = resolved.string();
    if (!ensureOutputDirectory(resolved, error)) {
      return emitFailure(options, primec::DiagnosticCode::OutputError, "Output error: ", error, 2);
    }
  }

  if (irBackend != nullptr) {
    const primec::IrBackendDiagnostics &diagnostics = irBackend->diagnostics();
    primec::IrBackendEmitResult emitResult;
    IrBackendRunFailure failure;
    if (!runIrBackend(*irBackend, program, options, emitResult, failure)) {
      if (failure.stage == IrBackendRunFailureStage::Lowering) {
        return emitFailure(options,
                           diagnostics.loweringDiagnosticCode,
                           diagnostics.loweringErrorPrefix,
                           failure.message,
                           2,
                           irBackendNotes(diagnostics));
      }
      if (failure.stage == IrBackendRunFailureStage::Validation) {
        return emitFailure(options,
                           diagnostics.validationDiagnosticCode,
                           diagnostics.validationErrorPrefix,
                           failure.message,
                           2,
                           irBackendNotes(diagnostics, "ir-validate"));
      }
      if (failure.stage == IrBackendRunFailureStage::Inlining) {
        return emitFailure(options,
                           diagnostics.inliningDiagnosticCode,
                           diagnostics.inliningErrorPrefix,
                           failure.message,
                           2,
                           irBackendNotes(diagnostics, "ir-inline"));
      }
      if (failure.stage == IrBackendRunFailureStage::OutputWrite) {
        return emitFailure(options,
                           primec::DiagnosticCode::OutputError,
                           "Failed to write output: ",
                           options.outputPath,
                           2);
      }
      const int emitFailureCode = diagnostics.backendTag == std::string_view("vm") ? 3 : 2;
      return emitFailure(options,
                         diagnostics.emitDiagnosticCode,
                         diagnostics.emitErrorPrefix,
                         failure.message,
                         emitFailureCode,
                         irBackendNotes(diagnostics));
    }
    return emitResult.exitCode;
  }

  if (!options.outputPath.empty()) {
    std::filesystem::path resolved = resolveOutputPath(options);
    options.outputPath = resolved.string();
    if (!ensureOutputDirectory(resolved, error)) {
      return emitFailure(options, primec::DiagnosticCode::OutputError, "Output error: ", error, 2);
    }
  }

  return emitFailure(options,
                     primec::DiagnosticCode::EmitError,
                     "Emit error: ",
                     "no backend available for emit kind " + options.emitKind,
                     2);
}
