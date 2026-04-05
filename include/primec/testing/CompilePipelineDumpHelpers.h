#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "primec/CompilePipeline.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrPreparation.h"
#include "primec/testing/TestScratch.h"

namespace primec::testing {

namespace detail {

inline std::string normalizeSemanticTargetName(std::string_view name) {
  std::string normalized(name);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const size_t specializationSuffix = normalized.find("__t");
  if (specializationSuffix != std::string::npos) {
    normalized.erase(specializationSuffix);
  }
  return normalized;
}

} // namespace detail

struct CompilePipelineBoundaryDumps {
  std::string astSemantic;
  std::string semanticProduct;
  std::string ir;
};

struct CompilePipelineBackendConformance {
  Options options;
  CompilePipelineOutput output;
  CompilePipelineErrorStage errorStage = CompilePipelineErrorStage::None;
  IrModule ir;
  std::string backendKind;
  IrBackendEmitResult emitResult;
  std::filesystem::path outputPath;

  const SemanticProgramDirectCallTarget *findDirectCallTarget(std::string_view scopePath,
                                                              std::string_view callName) const {
    const std::string normalizedCallName = detail::normalizeSemanticTargetName(callName);
    const auto it =
        std::find_if(output.semanticProgram.directCallTargets.begin(),
                     output.semanticProgram.directCallTargets.end(),
                     [&](const SemanticProgramDirectCallTarget &entry) {
                       return entry.scopePath == scopePath &&
                              (entry.callName == callName ||
                               detail::normalizeSemanticTargetName(entry.callName) == normalizedCallName);
                     });
    if (it == output.semanticProgram.directCallTargets.end()) {
      return nullptr;
    }
    return &*it;
  }

  const SemanticProgramMethodCallTarget *findMethodCallTarget(std::string_view scopePath,
                                                              std::string_view methodName) const {
    const auto it =
        std::find_if(output.semanticProgram.methodCallTargets.begin(),
                     output.semanticProgram.methodCallTargets.end(),
                     [&](const SemanticProgramMethodCallTarget &entry) {
                       return entry.scopePath == scopePath && entry.methodName == methodName;
                     });
    if (it == output.semanticProgram.methodCallTargets.end()) {
      return nullptr;
    }
    return &*it;
  }
};

namespace detail {

inline std::vector<std::string> defaultCompilePipelineTestingEffects() {
  return {"io_out", "io_err", "heap_alloc"};
}

struct PreparedCompilePipelineIrState {
  Options options;
  CompilePipelineOutput output;
  CompilePipelineErrorStage errorStage = CompilePipelineErrorStage::None;
  IrModule ir;
  std::string backendKind;
};

inline std::filesystem::path makeCompilePipelineDumpSourcePath() {
  static std::atomic<unsigned long long> counter{0};
  const auto nonce =
      std::to_string(static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
  return testScratchPath(
      "compile_pipeline_dumps/primestruct-dump-" + nonce + "-" + std::to_string(counter++) + ".prime");
}

inline bool captureCompilePipelineDumpStageFromPath(const std::filesystem::path &sourcePath,
                                                    const std::string &entryPath,
                                                    std::string_view dumpStage,
                                                    std::string &dump,
                                                    std::string &error,
                                                    CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  primec::Options options;
  options.inputPath = sourcePath.string();
  options.entryPath = entryPath;
  options.emitKind = "native";
  options.wasmProfile = "wasi";
  options.defaultEffects = detail::defaultCompilePipelineTestingEffects();
  options.entryDefaultEffects = options.defaultEffects;
  options.dumpStage = std::string(dumpStage);
  options.collectDiagnostics = diagnosticInfo != nullptr;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, diagnosticInfo);
  if (!ok) {
    return false;
  }

  dump = std::move(output.dumpOutput);
  return output.hasDumpOutput;
}

} // namespace detail

inline bool captureSemanticBoundaryDumpsForTesting(const std::string &source,
                                                   const std::string &entryPath,
                                                   CompilePipelineBoundaryDumps &dumps,
                                                   std::string &error) {
  const std::filesystem::path sourcePath = detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(sourcePath);
    if (!file) {
      error = "failed to write compile-pipeline boundary test source";
      return false;
    }
    file << source;
  }

  bool ok = detail::captureCompilePipelineDumpStageFromPath(
      sourcePath, entryPath, "ast-semantic", dumps.astSemantic, error);
  if (ok) {
    ok = detail::captureCompilePipelineDumpStageFromPath(
        sourcePath, entryPath, "semantic-product", dumps.semanticProduct, error);
  }
  if (ok) {
    ok = detail::captureCompilePipelineDumpStageFromPath(sourcePath, entryPath, "ir", dumps.ir, error);
  }

  std::error_code ec;
  std::filesystem::remove(sourcePath, ec);
  return ok;
}

inline bool prepareCompilePipelineIr(const std::string &source,
                                     const std::string &entryPath,
                                     std::string_view emitKind,
                                     detail::PreparedCompilePipelineIrState &prepared,
                                     std::string &error,
                                     CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  const std::filesystem::path sourcePath = detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(sourcePath);
    if (!file) {
      error = "failed to write compile-pipeline IR test source";
      return false;
    }
    file << source;
  }

  prepared = {};
  prepared.options.inputPath = sourcePath.string();
  prepared.options.entryPath = entryPath;
  prepared.options.emitKind = std::string(emitKind);
  prepared.options.wasmProfile = "wasi";
  prepared.options.defaultEffects = detail::defaultCompilePipelineTestingEffects();
  prepared.options.entryDefaultEffects = prepared.options.defaultEffects;
  prepared.options.collectDiagnostics = diagnosticInfo != nullptr;
  primec::addDefaultStdlibInclude(prepared.options.inputPath, prepared.options.importPaths);

  const bool ok = primec::runCompilePipeline(
      prepared.options, prepared.output, prepared.errorStage, error, diagnosticInfo);
  if (!ok) {
    std::error_code ec;
    std::filesystem::remove(sourcePath, ec);
    return false;
  }

  if (!prepared.output.hasSemanticProgram) {
    error = "compile pipeline did not publish semantic product";
    std::error_code ec;
    std::filesystem::remove(sourcePath, ec);
    return false;
  }

  prepared.backendKind = std::string(primec::resolveIrBackendEmitKind(emitKind));
  const primec::IrBackend *backend = primec::findIrBackend(prepared.backendKind);
  if (backend == nullptr) {
    error = "unknown IR backend for compile-pipeline IR test: " + std::string(emitKind);
    std::error_code ec;
    std::filesystem::remove(sourcePath, ec);
    return false;
  }

  primec::IrPreparationFailure failure;
  if (!primec::prepareIrModule(prepared.output.program,
                               &prepared.output.semanticProgram,
                               prepared.options,
                               backend->validationTarget(prepared.options),
                               prepared.ir,
                               failure)) {
    error = failure.message;
    std::error_code ec;
    std::filesystem::remove(sourcePath, ec);
    return false;
  }

  std::error_code ec;
  std::filesystem::remove(sourcePath, ec);
  return true;
}

inline bool runCompilePipelineBackendConformanceForTesting(const std::string &source,
                                                           const std::string &entryPath,
                                                           std::string_view emitKind,
                                                           CompilePipelineBackendConformance &conformance,
                                                           std::string &error,
                                                           CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  conformance = {};
  detail::PreparedCompilePipelineIrState prepared;
  if (!prepareCompilePipelineIr(
          source, entryPath, emitKind, prepared, error, diagnosticInfo)) {
    return false;
  }

  conformance.options = std::move(prepared.options);
  conformance.output = std::move(prepared.output);
  conformance.errorStage = prepared.errorStage;
  conformance.ir = std::move(prepared.ir);
  conformance.backendKind = std::move(prepared.backendKind);

  const IrBackend *backend = findIrBackend(conformance.backendKind);
  if (backend == nullptr) {
    error = "unknown IR backend for compile-pipeline backend conformance test: " + std::string(emitKind);
    return false;
  }

  IrBackendEmitOptions options;
  options.inputPath = "semantic_product_" + conformance.backendKind + "_boundary.prime";
  if (backend->requiresOutputPath()) {
    conformance.outputPath = testScratchPath("compile_pipeline_backend_conformance/" +
                                             conformance.backendKind + "_boundary");
    std::error_code ec;
    std::filesystem::create_directories(conformance.outputPath.parent_path(), ec);
    if (ec) {
      error = "failed to create backend conformance output directory";
      return false;
    }
    std::filesystem::remove(conformance.outputPath, ec);
    options.outputPath = conformance.outputPath.string();
  }

  return backend->emit(conformance.ir, options, conformance.emitResult, error);
}

} // namespace primec::testing
