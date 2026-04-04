#include "primec/CliDriver.h"
#include "primec/CompilePipeline.h"
#include "primec/Diagnostics.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrPreparation.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"

#include <filesystem>
#include <iostream>
#include <optional>
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

enum class IrBackendRunFailureStage {
  None,
  Emit,
  OutputWrite,
};

struct IrBackendRunFailure {
  IrBackendRunFailureStage stage = IrBackendRunFailureStage::None;
  std::string message;
  std::optional<primec::CliFailure> cliFailure;
};

bool runIrBackend(const primec::IrBackend &backend,
                  primec::Program &program,
                  const primec::SemanticProgram *semanticProgram,
                  const primec::Options &options,
                  primec::IrBackendEmitResult &result,
                  IrBackendRunFailure &failure) {
  failure = {};
  const primec::IrBackendDiagnostics &diagnostics = backend.diagnostics();

  const primec::IrValidationTarget validationTarget = backend.validationTarget(options);
  primec::IrModule ir;
  primec::IrPreparationFailure prepFailure;
  if (!primec::prepareIrModule(program, semanticProgram, options, validationTarget, ir, prepFailure)) {
    failure.cliFailure = primec::describeIrPreparationFailure(prepFailure, backend);
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
                << "[--dump-stage pre_ast|ast|ast-semantic|semantic-product|type-graph|ir] [-- <program args...>]\n"
                << "Dump-stage note: lowering-facing dumps now include semantic-product between ast-semantic and ir.\n";
    }
    return 2;
  }
  if (options.listTransforms) {
    primec::printTransformList(std::cout);
    return 0;
  }
  std::string error;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput pipelineOutput;
  primec::CompilePipelineDiagnosticInfo pipelineDiagnosticInfo;
  primec::CompilePipelineErrorStage pipelineError = primec::CompilePipelineErrorStage::None;
  if (!primec::runCompilePipeline(options, pipelineOutput, pipelineError, error, &pipelineDiagnosticInfo)) {
    const primec::CompilePipelineErrorStage failureStage =
        pipelineOutput.hasFailure ? pipelineOutput.failure.stage : pipelineError;
    const std::string &failureMessage = pipelineOutput.hasFailure ? pipelineOutput.failure.message : error;
    const primec::CompilePipelineDiagnosticInfo &failureDiagnosticInfo =
        pipelineOutput.hasFailure ? pipelineOutput.failure.diagnosticInfo : pipelineDiagnosticInfo;
    return primec::emitCliFailure(std::cerr,
                                  options,
                                  primec::describeCompilePipelineFailure(
                                      failureStage, failureMessage, pipelineOutput, failureDiagnosticInfo));
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
      primec::CliFailure outputFailure;
      outputFailure.code = primec::DiagnosticCode::OutputError;
      outputFailure.plainPrefix = "Output error: ";
      outputFailure.message = error;
      return primec::emitCliFailure(std::cerr, options, outputFailure);
    }
  }

  if (irBackend != nullptr) {
    const primec::IrBackendDiagnostics &diagnostics = irBackend->diagnostics();
    primec::IrBackendEmitResult emitResult;
    IrBackendRunFailure failure;
    const primec::SemanticProgram *semanticProgram =
        pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr;
    if (!runIrBackend(*irBackend, program, semanticProgram, options, emitResult, failure)) {
      if (failure.cliFailure.has_value()) {
        return primec::emitCliFailure(std::cerr, options, *failure.cliFailure);
      }
      if (failure.stage == IrBackendRunFailureStage::OutputWrite) {
        primec::CliFailure outputFailure;
        outputFailure.code = primec::DiagnosticCode::OutputError;
        outputFailure.plainPrefix = "Failed to write output: ";
        outputFailure.message = options.outputPath;
        return primec::emitCliFailure(std::cerr, options, outputFailure);
      }
      const int emitFailureCode = diagnostics.backendTag == std::string_view("vm") ? 3 : 2;
      primec::CliFailure emitFailure;
      emitFailure.code = diagnostics.emitDiagnosticCode;
      emitFailure.plainPrefix = diagnostics.emitErrorPrefix;
      emitFailure.message = failure.message;
      emitFailure.exitCode = emitFailureCode;
      emitFailure.notes = primec::makeIrBackendNotes(diagnostics);
      return primec::emitCliFailure(std::cerr, options, emitFailure);
    }
    return emitResult.exitCode;
  }

  if (!options.outputPath.empty()) {
    std::filesystem::path resolved = resolveOutputPath(options);
    options.outputPath = resolved.string();
    if (!ensureOutputDirectory(resolved, error)) {
      primec::CliFailure outputFailure;
      outputFailure.code = primec::DiagnosticCode::OutputError;
      outputFailure.plainPrefix = "Output error: ";
      outputFailure.message = error;
      return primec::emitCliFailure(std::cerr, options, outputFailure);
    }
  }

  primec::CliFailure emitFailure;
  emitFailure.code = primec::DiagnosticCode::EmitError;
  emitFailure.plainPrefix = "Emit error: ";
  emitFailure.message = "no backend available for emit kind " + options.emitKind;
  return primec::emitCliFailure(std::cerr, options, emitFailure);
}
