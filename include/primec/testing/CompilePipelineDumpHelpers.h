#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "primec/CompilePipeline.h"
#include "primec/testing/TestScratch.h"

namespace primec::testing {

struct CompilePipelineBoundaryDumps {
  std::string astSemantic;
  std::string semanticProduct;
  std::string ir;
};

namespace detail {

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
  options.defaultEffects = {"io_out", "io_err"};
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

inline bool captureCompilePipelineDumpForTesting(const std::string &source,
                                                 const std::string &entryPath,
                                                 std::string_view dumpStage,
                                                 std::string &dump,
                                                 std::string &error,
                                                 CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  const std::filesystem::path sourcePath = detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(sourcePath);
    if (!file) {
      error = "failed to write compile-pipeline dump test source";
      return false;
    }
    file << source;
  }

  const bool ok = detail::captureCompilePipelineDumpStageFromPath(
      sourcePath, entryPath, dumpStage, dump, error, diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(sourcePath, ec);
  return ok;
}

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

} // namespace primec::testing
