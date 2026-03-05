#include "primec/CompilePipeline.h"
#include "primec/Diagnostics.h"
#include "primec/Emitter.h"
#include "primec/ExternalTooling.h"
#include "primec/GlslEmitter.h"
#include "primec/IrInliner.h"
#include "primec/IrLowerer.h"
#include "primec/IrSerializer.h"
#include "primec/IrValidation.h"
#include "primec/NativeEmitter.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "primec/TransformRegistry.h"
#include "primec/Vm.h"
#include "primec/WasmEmitter.h"

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

void replaceAll(std::string &text, std::string_view from, std::string_view to) {
  if (from.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
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

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &contents) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(reinterpret_cast<const char *>(contents.data()), static_cast<std::streamsize>(contents.size()));
  return file.good();
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
      std::cerr << "Usage: primec [--emit=cpp|exe|native|ir|vm|glsl|spirv|wasm] <input.prime> [-o <output>] "
                   "[--entry /path] [--import-path <dir>] [-I <dir>] "
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

  if (options.emitKind == "vm") {
    primec::IrLowerer lowerer;
    primec::IrModule ir;
    if (!lowerer.lower(program,
                       options.entryPath,
                       options.defaultEffects,
                       options.entryDefaultEffects,
                       ir,
                       error)) {
      std::string vmError = error;
      replaceAll(vmError, "native backend", "vm backend");
      return emitFailure(
          options, primec::DiagnosticCode::LoweringError, "VM lowering error: ", vmError, 2, {"backend: vm"});
    }
    if (!primec::validateIrModule(ir, primec::IrValidationTarget::Vm, error)) {
      return emitFailure(options,
                         primec::DiagnosticCode::LoweringError,
                         "VM IR validation error: ",
                         error,
                         2,
                         {"backend: vm", "stage: ir-validate"});
    }
    if (options.inlineIrCalls) {
      if (!primec::inlineIrModuleCalls(ir, error)) {
        return emitFailure(options,
                           primec::DiagnosticCode::LoweringError,
                           "VM IR inlining error: ",
                           error,
                           2,
                           {"backend: vm", "stage: ir-inline"});
      }
      if (!primec::validateIrModule(ir, primec::IrValidationTarget::Vm, error)) {
        return emitFailure(options,
                           primec::DiagnosticCode::LoweringError,
                           "VM IR validation error: ",
                           error,
                           2,
                           {"backend: vm", "stage: ir-validate"});
      }
    }
    primec::Vm vm;
    std::vector<std::string_view> args;
    args.reserve(1 + options.programArgs.size());
    args.push_back(options.inputPath);
    for (const auto &arg : options.programArgs) {
      args.push_back(arg);
    }
    uint64_t result = 0;
    if (!vm.execute(ir, result, error, args)) {
      return emitFailure(options, primec::DiagnosticCode::RuntimeError, "VM error: ", error, 3, {"backend: vm"});
    }
    return static_cast<int>(static_cast<int32_t>(result));
  }

  if (!options.outputPath.empty()) {
    std::filesystem::path resolved = resolveOutputPath(options);
    options.outputPath = resolved.string();
    if (!ensureOutputDirectory(resolved, error)) {
      return emitFailure(options, primec::DiagnosticCode::OutputError, "Output error: ", error, 2);
    }
  }

  if (options.emitKind == "native") {
    primec::IrLowerer lowerer;
    primec::IrModule ir;
    if (!lowerer.lower(program,
                       options.entryPath,
                       options.defaultEffects,
                       options.entryDefaultEffects,
                       ir,
                       error)) {
      return emitFailure(
          options, primec::DiagnosticCode::LoweringError, "Native lowering error: ", error, 2, {"backend: native"});
    }
    if (!primec::validateIrModule(ir, primec::IrValidationTarget::Native, error)) {
      return emitFailure(options,
                         primec::DiagnosticCode::LoweringError,
                         "Native IR validation error: ",
                         error,
                         2,
                         {"backend: native", "stage: ir-validate"});
    }
    if (options.inlineIrCalls) {
      if (!primec::inlineIrModuleCalls(ir, error)) {
        return emitFailure(options,
                           primec::DiagnosticCode::LoweringError,
                           "Native IR inlining error: ",
                           error,
                           2,
                           {"backend: native", "stage: ir-inline"});
      }
      if (!primec::validateIrModule(ir, primec::IrValidationTarget::Native, error)) {
        return emitFailure(options,
                           primec::DiagnosticCode::LoweringError,
                           "Native IR validation error: ",
                           error,
                           2,
                           {"backend: native", "stage: ir-validate"});
      }
    }
    primec::NativeEmitter nativeEmitter;
    if (!nativeEmitter.emitExecutable(ir, options.outputPath, error)) {
      return emitFailure(options, primec::DiagnosticCode::EmitError, "Native emit error: ", error, 2, {"backend: native"});
    }
    return 0;
  }

  if (options.emitKind == "ir") {
    primec::IrLowerer lowerer;
    primec::IrModule ir;
    if (!lowerer.lower(program,
                       options.entryPath,
                       options.defaultEffects,
                       options.entryDefaultEffects,
                       ir,
                       error)) {
      return emitFailure(options, primec::DiagnosticCode::LoweringError, "IR lowering error: ", error, 2, {"backend: ir"});
    }
    if (!primec::validateIrModule(ir, primec::IrValidationTarget::Any, error)) {
      return emitFailure(options,
                         primec::DiagnosticCode::IrSerializeError,
                         "IR validation error: ",
                         error,
                         2,
                         {"backend: ir", "stage: ir-validate"});
    }
    if (options.inlineIrCalls) {
      if (!primec::inlineIrModuleCalls(ir, error)) {
        return emitFailure(options,
                           primec::DiagnosticCode::IrSerializeError,
                           "IR inlining error: ",
                           error,
                           2,
                           {"backend: ir", "stage: ir-inline"});
      }
      if (!primec::validateIrModule(ir, primec::IrValidationTarget::Any, error)) {
        return emitFailure(options,
                           primec::DiagnosticCode::IrSerializeError,
                           "IR validation error: ",
                           error,
                           2,
                           {"backend: ir", "stage: ir-validate"});
      }
    }
    std::vector<uint8_t> data;
    if (!primec::serializeIr(ir, data, error)) {
      return emitFailure(options, primec::DiagnosticCode::IrSerializeError, "IR serialize error: ", error, 2);
    }
    if (!writeBinaryFile(options.outputPath, data)) {
      return emitFailure(options,
                         primec::DiagnosticCode::OutputError,
                         "Failed to write output: ",
                         options.outputPath,
                         2);
    }
    return 0;
  }

  if (options.emitKind == "glsl") {
    primec::GlslEmitter glslEmitter;
    std::string glslSource;
    if (!glslEmitter.emitSource(program, options.entryPath, glslSource, error)) {
      return emitFailure(options, primec::DiagnosticCode::EmitError, "GLSL emit error: ", error, 2, {"backend: glsl"});
    }
    if (!writeFile(options.outputPath, glslSource)) {
      return emitFailure(options,
                         primec::DiagnosticCode::OutputError,
                         "Failed to write output: ",
                         options.outputPath,
                         2);
    }
    return 0;
  }

  if (options.emitKind == "spirv") {
    primec::GlslEmitter glslEmitter;
    std::string glslSource;
    if (!glslEmitter.emitSource(program, options.entryPath, glslSource, error)) {
      return emitFailure(options, primec::DiagnosticCode::EmitError, "GLSL emit error: ", error, 2, {"backend: glsl"});
    }
    std::string spirvSource = injectComputeLayout(glslSource);
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

  if (options.emitKind == "wasm") {
    primec::IrLowerer lowerer;
    primec::IrModule ir;
    if (!lowerer.lower(program,
                       options.entryPath,
                       options.defaultEffects,
                       options.entryDefaultEffects,
                       ir,
                       error)) {
      return emitFailure(
          options, primec::DiagnosticCode::LoweringError, "Wasm lowering error: ", error, 2, {"backend: wasm"});
    }
    if (!primec::validateIrModule(ir, primec::IrValidationTarget::Any, error)) {
      return emitFailure(options,
                         primec::DiagnosticCode::LoweringError,
                         "Wasm IR validation error: ",
                         error,
                         2,
                         {"backend: wasm", "stage: ir-validate"});
    }
    if (options.inlineIrCalls) {
      if (!primec::inlineIrModuleCalls(ir, error)) {
        return emitFailure(options,
                           primec::DiagnosticCode::LoweringError,
                           "Wasm IR inlining error: ",
                           error,
                           2,
                           {"backend: wasm", "stage: ir-inline"});
      }
      if (!primec::validateIrModule(ir, primec::IrValidationTarget::Any, error)) {
        return emitFailure(options,
                           primec::DiagnosticCode::LoweringError,
                           "Wasm IR validation error: ",
                           error,
                           2,
                           {"backend: wasm", "stage: ir-validate"});
      }
    }
    primec::WasmEmitter wasmEmitter;
    std::vector<uint8_t> data;
    if (!wasmEmitter.emitModule(ir, data, error)) {
      return emitFailure(options, primec::DiagnosticCode::EmitError, "Wasm emit error: ", error, 2, {"backend: wasm"});
    }
    if (!writeBinaryFile(options.outputPath, data)) {
      return emitFailure(options,
                         primec::DiagnosticCode::OutputError,
                         "Failed to write output: ",
                         options.outputPath,
                         2);
    }
    return 0;
  }

  if (auto softwareType = scanSoftwareNumericTypes(program)) {
    return emitFailure(options,
                       primec::DiagnosticCode::EmitError,
                       "C++ emit error: ",
                       "software numeric types are not supported: " + *softwareType,
                       2,
                       {"backend: cpp"});
  }

  primec::Emitter emitter;
  std::string cppSource = emitter.emitCpp(program, options.entryPath);

  if (options.emitKind == "cpp") {
    if (!writeFile(options.outputPath, cppSource)) {
      return emitFailure(options,
                         primec::DiagnosticCode::OutputError,
                         "Failed to write output: ",
                         options.outputPath,
                         2);
    }
    return 0;
  }

  std::filesystem::path outputPath(options.outputPath);
  std::filesystem::path cppPath = outputPath;
  cppPath.replace_extension(".cpp");
  if (!writeFile(cppPath.string(), cppSource)) {
    return emitFailure(options,
                       primec::DiagnosticCode::OutputError,
                       "Failed to write intermediate C++: ",
                       cppPath.string(),
                       2);
  }

  if (!primec::compileCppExecutable(processRunner, cppPath, outputPath)) {
    return emitFailure(options,
                       primec::DiagnosticCode::EmitError,
                       "",
                       "Failed to compile output executable",
                       3,
                       {"backend: cpp"});
  }

  return 0;
}
