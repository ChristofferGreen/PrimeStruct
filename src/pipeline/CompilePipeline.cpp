#include "primec/pipeline/CompilePipeline.h"
#include "primec/support/CompileContext.h"

#include "../frontend/ExpandedSourceBuilder.h"

#include "primec/ast/AstMemory.h"
#include "primec/ast/AstPrinter.h"
#include "primec/frontend/ImportResolver.h"
#include "primec/backend/IrBackendProfiles.h"
#include "primec/ir/IrPrinter.h"
#include "primec/frontend/Lexer.h"
#include "primec/frontend/Parser.h"
#include "primec/semantics/Semantics.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/semantics/SemanticsBenchmark.h"
#include "primec/support/SourceLocationMapper.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/frontend/StdlibSymbolManifest.h"
#include "primec/support/TextFilterPipeline.h"
#include "primec/support/TransformRules.h"
#include "../semantics/TypeResolutionGraph.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include "primec/support/BenchmarkSink.h"
#include "CompilePipelineInternal.h"

namespace primec {
using namespace compile_pipeline_detail;


void addDefaultStdlibInclude(const std::string &inputPath, std::vector<std::string> &importPaths) {
  auto addFromBase = [&](const std::filesystem::path &base) -> bool {
    std::error_code ec;
    std::filesystem::path dir = base;
    if (!std::filesystem::is_directory(dir, ec)) {
      dir = dir.parent_path();
    }
    for (std::filesystem::path current = dir; !current.empty(); current = current.parent_path()) {
      std::filesystem::path candidate = current / "stdlib";
      if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
        std::filesystem::path absoluteCandidate = std::filesystem::absolute(candidate, ec);
        std::string candidateText = absoluteCandidate.string();
        for (const auto &path : importPaths) {
          std::filesystem::path existing = std::filesystem::absolute(path, ec);
          if (!ec && std::filesystem::equivalent(existing, absoluteCandidate, ec)) {
            return true;
          }
          if (path == candidateText) {
            return true;
          }
        }
        importPaths.push_back(candidateText);
        return true;
      }
      if (current == current.root_path()) {
        break;
      }
    }
    return false;
  };

  if (!inputPath.empty()) {
    std::error_code ec;
    std::filesystem::path resolved = std::filesystem::absolute(inputPath, ec);
    if (ec) {
      resolved = std::filesystem::path(inputPath);
    }
    if (addFromBase(resolved)) {
      return;
    }
  }

  std::error_code ec;
  std::filesystem::path cwd = std::filesystem::current_path(ec);
  if (!ec) {
    addFromBase(cwd);
  }
}

bool runCompilePipeline(const Options &options,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error,
                        CompilePipelineDiagnosticInfo *diagnosticInfo) {
  CompilePipelineBenchmarkConfig benchmarkConfig;
  const CompilePipelineRunConfig runConfig =
      makeCompilePipelineRunConfigFromOptions(options, benchmarkConfig);
  return runCompilePipeline(options, runConfig, output, errorStage, error, diagnosticInfo);
}

namespace {

CompilePipelineFailureResult makeCompilePipelineFailureResult(
    CompilePipelineOutput &&output,
    CompilePipelineErrorStage errorStage,
    const std::string &error,
    const CompilePipelineDiagnosticInfo *diagnosticInfo) {
  CompilePipelineFailureResult result;
  result.program = std::move(output.program);
  result.semanticProgram = std::move(output.semanticProgram);
  result.hasSemanticProgram = output.hasSemanticProgram;
  result.semanticProductDecision = output.semanticProductDecision;
  result.semanticProductRequested = output.semanticProductRequested;
  result.semanticProductBuilt = output.semanticProductBuilt;
  result.semanticPhaseCounters = output.semanticPhaseCounters;
  result.hasSemanticPhaseCounters = output.hasSemanticPhaseCounters;
  result.expandedSource = std::move(output.expandedSource);
  result.filteredSource = std::move(output.filteredSource);
  result.dumpOutput = std::move(output.dumpOutput);
  result.hasDumpOutput = output.hasDumpOutput;
  result.failure = std::move(output.failure);
  if (!output.hasFailure) {
    result.failure.stage = errorStage;
    result.failure.message = error;
    if (diagnosticInfo != nullptr) {
      result.failure.diagnosticInfo = *diagnosticInfo;
    }
  }
  if (result.failure.message.empty()) {
    result.failure.message = error;
  }
  return result;
}

} // namespace

CompilePipelineResult runCompilePipelineResult(
    const Options &options,
    CompilePipelineErrorStage &errorStage,
    std::string &error,
    CompilePipelineDiagnosticInfo *diagnosticInfo) {
  CompilePipelineBenchmarkConfig benchmarkConfig;
  const CompilePipelineRunConfig runConfig =
      makeCompilePipelineRunConfigFromOptions(options, benchmarkConfig);
  return runCompilePipelineResult(options, runConfig, errorStage, error, diagnosticInfo);
}

CompilePipelineResult runCompilePipelineResult(
    const Options &options,
    const CompilePipelineRunConfig &runConfig,
    CompilePipelineErrorStage &errorStage,
    std::string &error,
    CompilePipelineDiagnosticInfo *diagnosticInfo) {
  CompilePipelineOutput output;
  if (runCompilePipeline(options, runConfig, output, errorStage, error, diagnosticInfo)) {
    return CompilePipelineSuccessResult{std::move(output)};
  }
  return makeCompilePipelineFailureResult(
      std::move(output), errorStage, error, diagnosticInfo);
}

bool runCompilePipeline(const Options &options,
                        const CompilePipelineRunConfig &runConfig,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error,
                        CompilePipelineDiagnosticInfo *diagnosticInfo) {
  // One compilation, one context.
  CompileContext compileContext;
  const CompileContext::Scope compileContextScope(compileContext);
  errorStage = CompilePipelineErrorStage::None;
  output = {};
  error.clear();
  CompilePipelineDiagnosticInfo capturedDiagnosticInfo;
  DiagnosticSink diagnosticSink(&capturedDiagnosticInfo);
  diagnosticSink.reset();
  if (diagnosticInfo != nullptr) {
    *diagnosticInfo = {};
  }
  const bool benchmarkAstHeapEstimate =
      std::getenv("PRIMEC_BENCHMARK_COMPILE_AST_HEAP_ESTIMATE") != nullptr;

  auto failPipeline = [&](CompilePipelineErrorStage stage,
                          const std::string &message,
                          const CompilePipelineDiagnosticInfo &info) -> bool {
    errorStage = stage;
    output.failure.stage = stage;
    output.failure.message = message;
    output.failure.diagnosticInfo = info;
    output.hasFailure = true;
    if (diagnosticInfo != nullptr) {
      *diagnosticInfo = info;
    }
    return false;
  };

  if (const std::string &registryError = stdlibSurfaceRegistryStartupError(); !registryError.empty()) {
    return failPipeline(CompilePipelineErrorStage::Semantic, registryError, capturedDiagnosticInfo);
  }

  CompilePipelineImportStageState importStage;
  if (!runCompilePipelineImportStage(options,
                                     importStage,
                                     error,
                                     diagnosticSink)) {
    return failPipeline(CompilePipelineErrorStage::Import, error, capturedDiagnosticInfo);
  }
  output.expandedSource = importStage.expandedSource;

  CompilePipelinePreParseStageState preParseStage;
  if (!runCompilePipelineTransformStage(options,
                                        importStage,
                                        preParseStage,
                                        error,
                                        diagnosticSink)) {
    return failPipeline(CompilePipelineErrorStage::Transform, error, capturedDiagnosticInfo);
  }

  output.filteredSource = preParseStage.filteredSource;

  const DumpStage dumpStage = parseDumpStage(options.dumpStage);

  if (dumpStage == DumpStage::PreAst) {
    output.dumpOutput = preParseStage.filteredSource;
    output.hasDumpOutput = true;
    return true;
  }

  CompilePipelineParsedProgramStageState parsedStage;
  if (!runCompilePipelineParseStage(options,
                                    importStage.expandedSource,
                                    preParseStage,
                                    parsedStage,
                                    error,
                                    diagnosticSink)) {
    return failPipeline(CompilePipelineErrorStage::Parse, error, capturedDiagnosticInfo);
  }
  output.program = std::move(parsedStage.program);

  if (benchmarkAstHeapEstimate) {
    emitProgramHeapEstimate(output.program, "post-parse-pre-semantics");
  }

  if (dumpStage != DumpStage::None && dumpStage != DumpStage::AstSemantic &&
      dumpStage != DumpStage::SemanticProduct && dumpStage != DumpStage::TypeGraph) {
    if (dumpStage == DumpStage::Ast) {
      AstPrinter printer;
      output.dumpOutput = printer.print(output.program);
      output.hasDumpOutput = true;
      return true;
    }
    if (dumpStage == DumpStage::Ir) {
      IrPrinter printer;
      output.dumpOutput = printer.print(output.program);
      output.hasDumpOutput = true;
      return true;
    }
    error = options.dumpStage;
    diagnosticSink.setSummary(error);
    return failPipeline(CompilePipelineErrorStage::UnsupportedDumpStage, error, capturedDiagnosticInfo);
  }

  if (!options.semanticTransformRules.empty()) {
    applySemanticTransformRules(output.program, options.semanticTransformRules);
  }

  if (dumpStage == DumpStage::TypeGraph) {
    semantics::TypeResolutionGraph graph;
    if (!semantics::buildTypeResolutionGraphForProgram(
            output.program, options.entryPath, options.semanticTransforms, error, graph)) {
      diagnosticSink.setSummary(error);
      return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
    }
    output.dumpOutput = semantics::formatTypeResolutionGraph(graph);
    output.hasDumpOutput = true;
    return true;
  }

  Semantics semantics;
  SemanticDiagnosticInfo semanticDiagnosticInfo;
  SemanticProgram semanticProgram;
  const CompilePipelineSemanticProductDecision semanticProductDecision =
      decideSemanticProductDecision(dumpStage, runConfig);
  const bool needsSemanticProduct = semanticProductDecisionRequestsBuild(semanticProductDecision);
  const CompilePipelineBenchmarkConfig *benchmarkConfig = runConfig.benchmark;
  SemanticProductBuildConfig semanticProductBuildConfig;
  const SemanticProductBuildConfig *semanticProductBuildConfigPtr = nullptr;
  if (benchmarkConfig != nullptr &&
      (benchmarkConfig->semanticNoFactEmission || benchmarkConfig->semanticFactFamiliesSpecified)) {
    semanticProductBuildConfig.disableAllCollectors = benchmarkConfig->semanticNoFactEmission;
    semanticProductBuildConfig.collectorAllowlistSpecified =
        benchmarkConfig->semanticFactFamiliesSpecified;
    semanticProductBuildConfig.collectorAllowlist = benchmarkConfig->semanticFactFamilies;
    for (const auto &collectorFamily : semanticProductBuildConfig.collectorAllowlist) {
      if (!isKnownSemanticCollectorFamily(collectorFamily)) {
        error = "unknown benchmark semantic collector family: " + collectorFamily;
        diagnosticSink.setSummary(error);
        return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
      }
    }
    semanticProductBuildConfigPtr = &semanticProductBuildConfig;
  }
  const uint32_t benchmarkSemanticDefinitionValidationWorkerCount =
      semanticDefinitionValidationWorkerCount(benchmarkConfig);
  SemanticPhaseCounters benchmarkSemanticPhaseCounters;
  const bool benchmarkSemanticCountersRequested =
      semanticBenchmarkCountersRequested(benchmarkConfig);
  SemanticPhaseCounters *benchmarkSemanticPhaseCountersPtr =
      benchmarkSemanticCountersRequested ? &benchmarkSemanticPhaseCounters : nullptr;
  const bool benchmarkSemanticConfigRequested =
      semanticBenchmarkValidationConfigRequested(benchmarkConfig,
                                                benchmarkSemanticDefinitionValidationWorkerCount);
  output.semanticProductDecision = semanticProductDecision;
  output.semanticProductRequested = needsSemanticProduct;
  bool semanticValidationOk = false;
  if (benchmarkSemanticConfigRequested || benchmarkSemanticCountersRequested) {
    SemanticValidationBenchmarkConfig benchmarkConfig;
    benchmarkConfig.definitionValidationWorkerCount = benchmarkSemanticDefinitionValidationWorkerCount;
    benchmarkConfig.disableMethodTargetMemoization =
        runConfig.benchmark->semanticDisableMethodTargetMemoization;
    benchmarkConfig.graphLocalAutoLegacyKeyShadow =
        runConfig.benchmark->semanticGraphLocalAutoLegacyKeyShadow;
    benchmarkConfig.graphLocalAutoLegacySideChannelShadow =
        runConfig.benchmark->semanticGraphLocalAutoLegacySideChannelShadow;
    benchmarkConfig.disableGraphLocalAutoDependencyScratchPmr =
        runConfig.benchmark->semanticDisableGraphLocalAutoDependencyScratchPmr;

    SemanticValidationBenchmarkObserver benchmarkObserver;
    benchmarkObserver.phaseCounters = benchmarkSemanticPhaseCountersPtr;
    benchmarkObserver.allocationCountersEnabled = runConfig.benchmark->semanticAllocationCounters;
    benchmarkObserver.rssCheckpointsEnabled = runConfig.benchmark->semanticRssCheckpoints;

    semanticValidationOk = validateSemanticsForBenchmark(output.program,
                                                         options.entryPath,
                                                         error,
                                                         options.defaultEffects,
                                                         options.entryDefaultEffects,
                                                         options.semanticTransforms,
                                                         &semanticDiagnosticInfo,
                                                         options.collectDiagnostics,
                                                         needsSemanticProduct ? &semanticProgram : nullptr,
                                                         semanticProductBuildConfigPtr,
                                                         benchmarkConfig,
                                                         benchmarkObserver,
                                                         &importStage.lazyStdlibModuleKeys);
  } else {
    semanticValidationOk = semantics.validate(output.program,
                                              options.entryPath,
                                              error,
                                              options.defaultEffects,
                                              options.entryDefaultEffects,
                                              options.semanticTransforms,
                                              &semanticDiagnosticInfo,
                                              options.collectDiagnostics,
                                              needsSemanticProduct ? &semanticProgram : nullptr,
                                              semanticProductBuildConfigPtr,
                                              &importStage.lazyStdlibModuleKeys);
  }
  if (!semanticValidationOk) {
    // A target/graphics-backend mismatch (e.g. glsl target without runtime
    // substrate) takes priority over any semantic failure below, including
    // the TODO-5228 lazy-import rewrite: it depends only on program.imports
    // (populated straight from parsing) and would fail regardless of
    // whether anything in the imported module resolves. Without this,
    // lazy stdlib import expansion could leave a wildcard-imported,
    // never-used graphics module with zero spliced definitions, semantic
    // validation would fail with "unknown import path" first, and the
    // target mismatch's more specific, actionable diagnostic would never
    // surface - masked by a confusing downstream error the target-support
    // check exists specifically to preempt.
    if (!validateGraphicsBackendSupport(output.program, options, error, &capturedDiagnosticInfo)) {
      return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
    }
    // TODO-5228: the closure-scan heuristic that decides which manifested
    // symbols to splice in is purely syntactic (a whole-word name scan),
    // so it can miss a symbol the program actually needed. When that
    // happens, nothing from the lazily-imported module ends up in the
    // compiled buffer, and the pre-existing "unknown import path: X"
    // check (which fires whenever a wildcard-imported root has zero
    // definitions) is what actually surfaces - not the more specific
    // "unknown call target"/"unknown identifier" a whole-file splice would
    // have produced. Rewrite it into the clearer, TODO-5228-specific
    // message the plan calls for, rather than letting a confusing
    // downstream error stand.
    for (const std::string &lazyKey : importStage.lazyStdlibModuleKeys) {
      // Exact match only: "unknown import path: " + lazyKey by itself means
      // the wildcard-imported lazy module produced zero definitions (the
      // closure scan's syntactic heuristic missed everything the program
      // needed). A *prefix* match would also catch an unrelated, genuinely
      // nonexistent sub-path like "/std/gfx/experimental/nope" (which
      // textually starts with the lazy module's own key but is a real,
      // distinct "no such import" error the rewrite must not mask). The
      // diagnostic echoes back the import path exactly as written, so a
      // wildcard import surfaces as "unknown import path: <key>/*" (with
      // the literal "/*") rather than bare "<key>" - match both forms, but
      // nothing else, so a lazy module imported as a bare wildcard still
      // gets rewritten to the clearer message.
      const std::string unknownImportMessage = "unknown import path: " + lazyKey;
      const std::string unknownImportWildcardMessage = unknownImportMessage + "/*";
      if (error == unknownImportMessage || error == unknownImportWildcardMessage) {
        error = "unknown symbol in imported library " + lazyKey +
                " (lazy stdlib import expansion could not find any "
                "manifested symbol matching a name referenced by this "
                "program; pass --whole-file-stdlib-imports to see the "
                "underlying diagnostic, or verify the spelling)";
        semanticDiagnosticInfo.message = error;
        break;
      }
    }
    if (semanticDiagnosticInfo.message.empty()) {
      semanticDiagnosticInfo.message = error;
    }
    mapAndSortDiagnosticReportSpansToSourceUnits(importStage.expandedSource,
                                                 semanticDiagnosticInfo);
    return failPipeline(CompilePipelineErrorStage::Semantic, error, semanticDiagnosticInfo);
  }
  if (benchmarkSemanticPhaseCountersPtr != nullptr) {
    output.semanticPhaseCounters = benchmarkSemanticPhaseCounters;
    output.hasSemanticPhaseCounters = true;
  }

  if (benchmarkAstHeapEstimate) {
    emitProgramHeapEstimate(output.program, "post-semantics");
  }

  if (needsSemanticProduct) {
    output.semanticProgram = std::move(semanticProgram);
    output.hasSemanticProgram = true;
    output.semanticProductBuilt = true;
  }

  if (!validateGraphicsBackendSupport(output.program, options, error, &capturedDiagnosticInfo)) {
    return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
  }

  if (dumpStage == DumpStage::SemanticProduct) {
    if (!output.hasSemanticProgram) {
      error = "semantic-product dump requested without semantic product";
      diagnosticSink.setSummary(error);
      return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
    }
    output.dumpOutput = formatSemanticProgram(output.semanticProgram);
    output.hasDumpOutput = true;
    return true;
  }

  if (dumpStage == DumpStage::AstSemantic) {
    AstPrinter printer;
    output.dumpOutput = printer.print(output.program);
    output.hasDumpOutput = true;
    return true;
  }

  return true;
}

} // namespace primec
