#include "primec/CompilePipeline.h"
#include "primec/Diagnostics.h"
#include "primec/ExternalTooling.h"
#include "primec/GlslEmitter.h"
#include "primec/IrBackends.h"
#include "primec/IrInliner.h"
#include "primec/IrLowerer.h"
#include "primec/IrValidation.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "primec/TransformRegistry.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {
bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg) {
  base.clear();
  arg.clear();
  const size_t open = text.find('<');
  if (open == std::string::npos || open == 0 || text.back() != '>') {
    return false;
  }
  base = text.substr(0, open);
  int depth = 0;
  size_t start = open + 1;
  for (size_t i = start; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth == 0) {
        if (i + 1 != text.size()) {
          return false;
        }
        arg = text.substr(start, i - start);
        return true;
      }
      --depth;
    }
  }
  return false;
}

bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  auto pushSegment = [&](size_t end) {
    size_t segStart = start;
    while (segStart < end && std::isspace(static_cast<unsigned char>(text[segStart]))) {
      ++segStart;
    }
    size_t segEnd = end;
    while (segEnd > segStart && std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
      --segEnd;
    }
    out.push_back(text.substr(segStart, segEnd - segStart));
  };
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (c == ',' && depth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  for (const auto &seg : out) {
    if (seg.empty()) {
      return false;
    }
  }
  return !out.empty();
}

std::optional<std::string> findSoftwareNumericType(const std::string &typeName) {
  auto isSoftwareNumericName = [](const std::string &name) {
    return name == "integer" || name == "decimal" || name == "complex";
  };
  if (typeName.empty()) {
    return std::nullopt;
  }
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg)) {
    if (isSoftwareNumericName(typeName)) {
      return typeName;
    }
    return std::nullopt;
  }
  if (isSoftwareNumericName(base)) {
    return base;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(arg, args)) {
    return std::nullopt;
  }
  for (const auto &nested : args) {
    if (auto found = findSoftwareNumericType(nested)) {
      return found;
    }
  }
  return std::nullopt;
}

std::optional<std::string> scanSoftwareNumericTypes(const primec::Program &program) {
  auto scanTransforms = [&](const std::vector<primec::Transform> &transforms) -> std::optional<std::string> {
    for (const auto &transform : transforms) {
      if (auto found = findSoftwareNumericType(transform.name)) {
        return found;
      }
      for (const auto &arg : transform.templateArgs) {
        if (auto found = findSoftwareNumericType(arg)) {
          return found;
        }
      }
    }
    return std::nullopt;
  };
  std::function<std::optional<std::string>(const primec::Expr &)> scanExpr;
  scanExpr = [&](const primec::Expr &expr) -> std::optional<std::string> {
    if (auto found = scanTransforms(expr.transforms)) {
      return found;
    }
    for (const auto &arg : expr.templateArgs) {
      if (auto found = findSoftwareNumericType(arg)) {
        return found;
      }
    }
    for (const auto &arg : expr.args) {
      if (auto found = scanExpr(arg)) {
        return found;
      }
    }
    for (const auto &arg : expr.bodyArguments) {
      if (auto found = scanExpr(arg)) {
        return found;
      }
    }
    return std::nullopt;
  };
  for (const auto &def : program.definitions) {
    if (auto found = scanTransforms(def.transforms)) {
      return found;
    }
    for (const auto &param : def.parameters) {
      if (auto found = scanExpr(param)) {
        return found;
      }
    }
    for (const auto &stmt : def.statements) {
      if (auto found = scanExpr(stmt)) {
        return found;
      }
    }
    if (def.returnExpr.has_value()) {
      if (auto found = scanExpr(*def.returnExpr)) {
        return found;
      }
    }
  }
  for (const auto &exec : program.executions) {
    if (auto found = scanTransforms(exec.transforms)) {
      return found;
    }
    for (const auto &arg : exec.arguments) {
      if (auto found = scanExpr(arg)) {
        return found;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      if (auto found = scanExpr(arg)) {
        return found;
      }
    }
  }
  return std::nullopt;
}

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

bool writeFile(const std::string &path, const std::string &contents) {
  std::ofstream file(path);
  if (!file) {
    return false;
  }
  file << contents;
  return file.good();
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

std::string injectComputeLayout(const std::string &glslSource) {
  const std::string layoutLine = "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
  size_t versionPos = glslSource.find("#version");
  if (versionPos == std::string::npos) {
    return "#version 450\n" + layoutLine + glslSource;
  }
  size_t lineEnd = glslSource.find('\n', versionPos);
  if (lineEnd == std::string::npos) {
    return glslSource + "\n" + layoutLine;
  }
  return glslSource.substr(0, lineEnd + 1) + layoutLine + glslSource.substr(lineEnd + 1);
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

  std::string error;
  primec::IrLowerer lowerer;
  primec::IrModule ir;
  if (!lowerer.lower(program,
                     options.entryPath,
                     options.defaultEffects,
                     options.entryDefaultEffects,
                     ir,
                     error)) {
    backend.normalizeLoweringError(error);
    failure.stage = IrBackendRunFailureStage::Lowering;
    failure.message = std::move(error);
    return false;
  }

  const primec::IrValidationTarget validationTarget = backend.validationTarget(options);
  if (!primec::validateIrModule(ir, validationTarget, error)) {
    failure.stage = IrBackendRunFailureStage::Validation;
    failure.message = std::move(error);
    return false;
  }

  if (options.inlineIrCalls) {
    if (!primec::inlineIrModuleCalls(ir, error)) {
      failure.stage = IrBackendRunFailureStage::Inlining;
      failure.message = std::move(error);
      return false;
    }
    if (!primec::validateIrModule(ir, validationTarget, error)) {
      failure.stage = IrBackendRunFailureStage::Validation;
      failure.message = std::move(error);
      return false;
    }
  }

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
      std::cerr
          << "Usage: primec [--emit=cpp|cpp-ir|exe|exe-ir|native|ir|vm|glsl|spirv|wasm|glsl-ir|spirv-ir] "
             "<input.prime> "
                   "[-o <output>] "
                   "[--entry /path] [--import-path <dir>] [-I <dir>] "
                   "[--wasm-profile wasi|browser] "
                   "[--text-transforms <list>] [--text-transform-rules <rules>] "
                   "[--semantic-transform-rules <rules>] [--semantic-transforms <list>] "
                   "[--transform-list <list>] [--no-text-transforms] [--no-semantic-transforms] "
                   "[--no-transforms] [--out-dir <dir>] [--list-transforms] [--emit-diagnostics] "
                   "[--collect-diagnostics] "
                   "[--default-effects <list>] [--ir-inline] "
                   "[--dump-stage pre_ast|ast|ast-semantic|ir] [-- <program args...>]\n";
    }
    return 2;
  }
  if (options.listTransforms) {
    printTransformList(std::cout);
    return 0;
  }
  const primec::ProcessRunner &processRunner = primec::systemProcessRunner();
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

  if ((options.emitKind == "cpp" || options.emitKind == "exe")) {
    if (auto softwareType = scanSoftwareNumericTypes(program)) {
      return emitFailure(options,
                         primec::DiagnosticCode::EmitError,
                         "C++ emit error: ",
                         "software numeric types are not supported: " + *softwareType,
                         2,
                         {"backend: cpp"});
    }
  }

  const primec::IrBackend *irBackend = primec::findIrBackend(options.emitKind);
  if (irBackend == nullptr && options.emitKind == "cpp") {
    irBackend = primec::findIrBackend("cpp-ir");
  } else if (irBackend == nullptr && options.emitKind == "exe") {
    irBackend = primec::findIrBackend("exe-ir");
  }

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

  if (options.emitKind == "glsl") {
    if (const primec::IrBackend *glslIrBackend = primec::findIrBackend("glsl-ir"); glslIrBackend != nullptr) {
      primec::IrBackendEmitResult emitResult;
      IrBackendRunFailure irFailure;
      if (runIrBackend(*glslIrBackend, program, options, emitResult, irFailure)) {
        return emitResult.exitCode;
      }
      if (irFailure.stage != IrBackendRunFailureStage::Emit) {
        const primec::IrBackendDiagnostics &diagnostics = glslIrBackend->diagnostics();
        if (irFailure.stage == IrBackendRunFailureStage::Lowering) {
          return emitFailure(options,
                             diagnostics.loweringDiagnosticCode,
                             diagnostics.loweringErrorPrefix,
                             irFailure.message,
                             2,
                             irBackendNotes(diagnostics));
        }
        if (irFailure.stage == IrBackendRunFailureStage::Validation) {
          return emitFailure(options,
                             diagnostics.validationDiagnosticCode,
                             diagnostics.validationErrorPrefix,
                             irFailure.message,
                             2,
                             irBackendNotes(diagnostics, "ir-validate"));
        }
        if (irFailure.stage == IrBackendRunFailureStage::Inlining) {
          return emitFailure(options,
                             diagnostics.inliningDiagnosticCode,
                             diagnostics.inliningErrorPrefix,
                             irFailure.message,
                             2,
                             irBackendNotes(diagnostics, "ir-inline"));
        }
        if (irFailure.stage == IrBackendRunFailureStage::OutputWrite) {
          return emitFailure(options,
                             primec::DiagnosticCode::OutputError,
                             "Failed to write output: ",
                             options.outputPath,
                             2);
        }
      }
    }

    std::string legacySource;
    std::string legacyError;
    primec::GlslEmitter glslEmitter;
    if (!glslEmitter.emitSource(program, options.entryPath, legacySource, legacyError)) {
      return emitFailure(options,
                         primec::DiagnosticCode::EmitError,
                         "GLSL emit error: ",
                         legacyError,
                         2,
                         {"backend: glsl"});
    }

    if (!writeFile(options.outputPath, legacySource)) {
      return emitFailure(options,
                         primec::DiagnosticCode::OutputError,
                         "Failed to write output: ",
                         options.outputPath,
                         2);
    }
    return 0;
  }

  if (options.emitKind == "spirv") {
    if (const primec::IrBackend *spirvIrBackend = primec::findIrBackend("spirv-ir"); spirvIrBackend != nullptr) {
      primec::IrBackendEmitResult emitResult;
      IrBackendRunFailure irFailure;
      if (runIrBackend(*spirvIrBackend, program, options, emitResult, irFailure)) {
        return emitResult.exitCode;
      }
      if (irFailure.stage != IrBackendRunFailureStage::Emit) {
        const primec::IrBackendDiagnostics &diagnostics = spirvIrBackend->diagnostics();
        if (irFailure.stage == IrBackendRunFailureStage::Lowering) {
          return emitFailure(options,
                             diagnostics.loweringDiagnosticCode,
                             diagnostics.loweringErrorPrefix,
                             irFailure.message,
                             2,
                             irBackendNotes(diagnostics));
        }
        if (irFailure.stage == IrBackendRunFailureStage::Validation) {
          return emitFailure(options,
                             diagnostics.validationDiagnosticCode,
                             diagnostics.validationErrorPrefix,
                             irFailure.message,
                             2,
                             irBackendNotes(diagnostics, "ir-validate"));
        }
        if (irFailure.stage == IrBackendRunFailureStage::Inlining) {
          return emitFailure(options,
                             diagnostics.inliningDiagnosticCode,
                             diagnostics.inliningErrorPrefix,
                             irFailure.message,
                             2,
                             irBackendNotes(diagnostics, "ir-inline"));
        }
        if (irFailure.stage == IrBackendRunFailureStage::OutputWrite) {
          return emitFailure(options,
                             primec::DiagnosticCode::OutputError,
                             "Failed to write output: ",
                             options.outputPath,
                             2);
        }
      }
    }

    std::string legacySource;
    std::string legacyError;
    primec::GlslEmitter glslEmitter;
    if (!glslEmitter.emitSource(program, options.entryPath, legacySource, legacyError)) {
      return emitFailure(options,
                         primec::DiagnosticCode::EmitError,
                         "GLSL emit error: ",
                         legacyError,
                         2,
                         {"backend: glsl"});
    }

    const std::string spirvSource = injectComputeLayout(legacySource);
    std::string toolName;
    if (!primec::findSpirvCompiler(processRunner, toolName)) {
      return emitFailure(options,
                         primec::DiagnosticCode::EmitError,
                         "SPIR-V emit error: ",
                         "glslangValidator or glslc not found",
                         2,
                         {"backend: spirv"});
    }

    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / ("primec_spirv_" + std::to_string(timestamp) + ".comp");
    if (!writeFile(tempPath.string(), spirvSource)) {
      return emitFailure(options,
                         primec::DiagnosticCode::OutputError,
                         "Failed to write output: ",
                         tempPath.string(),
                         2);
    }

    bool ok = primec::compileSpirv(processRunner, toolName, tempPath, options.outputPath);
    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
    if (!ok) {
      return emitFailure(options,
                         primec::DiagnosticCode::EmitError,
                         "SPIR-V emit error: ",
                         "tool invocation failed",
                         2,
                         {"backend: spirv"});
    }
    return 0;
  }

  return emitFailure(options,
                     primec::DiagnosticCode::EmitError,
                     "Emit error: ",
                     "no backend available for emit kind " + options.emitKind,
                     2);
}
