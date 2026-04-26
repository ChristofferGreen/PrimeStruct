#pragma once

#include "primec/Ast.h"
#include "primec/Diagnostics.h"
#include "primec/Options.h"
#include "primec/SemanticProduct.h"
#include "primec/Semantics.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace primec {

enum class CompilePipelineErrorStage {
  None,
  Import,
  Transform,
  Parse,
  UnsupportedDumpStage,
  Semantic,
};

using CompilePipelineDiagnosticInfo = DiagnosticSinkReport;

enum class CompilePipelineSemanticProductDecision {
  RequireForConsumingPath,
  SkipForAstSemanticDump,
  SkipForNonConsumingPath,
  ForcedOnForBenchmark,
  ForcedOffForBenchmark,
};

struct CompilePipelineFailure {
  CompilePipelineErrorStage stage = CompilePipelineErrorStage::None;
  std::string message;
  CompilePipelineDiagnosticInfo diagnosticInfo;
};

struct CompilePipelineBenchmarkConfig {
  std::optional<bool> forceSemanticProduct;
  bool semanticNoFactEmission = false;
  bool semanticFactFamiliesSpecified = false;
  std::vector<std::string> semanticFactFamilies;
  bool semanticTwoChunkDefinitionValidation = false;
  std::optional<uint32_t> semanticDefinitionValidationWorkerCount;
  bool semanticPhaseCounters = false;
  bool semanticAllocationCounters = false;
  bool semanticRssCheckpoints = false;
  bool semanticDisableMethodTargetMemoization = false;
  bool semanticGraphLocalAutoLegacyKeyShadow = false;
  bool semanticGraphLocalAutoLegacySideChannelShadow = false;
  bool semanticDisableGraphLocalAutoDependencyScratchPmr = false;
};

struct CompilePipelineRunConfig {
  bool skipSemanticProductForNonConsumingPath = false;
  const CompilePipelineBenchmarkConfig *benchmark = nullptr;
};

struct CompilePipelineOutput {
  Program program;
  SemanticProgram semanticProgram;
  bool hasSemanticProgram = false;
  CompilePipelineSemanticProductDecision semanticProductDecision =
      CompilePipelineSemanticProductDecision::RequireForConsumingPath;
  bool semanticProductRequested = false;
  bool semanticProductBuilt = false;
  SemanticPhaseCounters semanticPhaseCounters;
  bool hasSemanticPhaseCounters = false;
  std::string filteredSource;
  std::string dumpOutput;
  bool hasDumpOutput = false;
  CompilePipelineFailure failure;
  bool hasFailure = false;
};

void addDefaultStdlibInclude(const std::string &inputPath, std::vector<std::string> &importPaths);

bool runCompilePipeline(const Options &options,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error,
                        CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr);

bool runCompilePipeline(const Options &options,
                        const CompilePipelineRunConfig &runConfig,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error,
                        CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr);

} // namespace primec
