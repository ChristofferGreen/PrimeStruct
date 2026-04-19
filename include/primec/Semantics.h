#pragma once

#include "primec/Ast.h"
#include "primec/Diagnostics.h"
#include "primec/SemanticProduct.h"

#include <cstdint>
#include <vector>

namespace primec {

using SemanticDiagnosticRelatedSpan = DiagnosticRelatedSpan;
using SemanticDiagnosticRecord = DiagnosticSinkRecord;
using SemanticDiagnosticInfo = DiagnosticSinkReport;

struct SemanticProductBuildConfig {
  bool disableAllCollectors = false;
  bool collectorAllowlistSpecified = false;
  std::vector<std::string> collectorAllowlist;
};

struct SemanticPhaseCounterSnapshot {
  uint64_t callsVisited = 0;
  uint64_t factsProduced = 0;
  uint64_t peakLocalMapSize = 0;
  uint64_t allocationCount = 0;
  uint64_t allocatedBytes = 0;
  uint64_t rssBeforeBytes = 0;
  uint64_t rssAfterBytes = 0;
};

struct SemanticPhaseCounters {
  SemanticPhaseCounterSnapshot validation;
  SemanticPhaseCounterSnapshot semanticProductBuild;
};

struct SemanticValidationBenchmarkConfig {
  uint32_t definitionValidationWorkerCount = 1;
  bool disableMethodTargetMemoization = false;
  bool graphLocalAutoLegacyKeyShadow = false;
  bool graphLocalAutoLegacySideChannelShadow = false;
  bool disableGraphLocalAutoDependencyScratchPmr = false;
};

struct SemanticValidationBenchmarkObserver {
  SemanticPhaseCounters *phaseCounters = nullptr;
  bool allocationCountersEnabled = false;
  bool rssCheckpointsEnabled = false;
};

class Semantics {
public:
  bool validate(Program &program,
                const std::string &entryPath,
                std::string &error,
                const std::vector<std::string> &defaultEffects,
                const std::vector<std::string> &entryDefaultEffects,
                const std::vector<std::string> &semanticTransforms = {},
                SemanticDiagnosticInfo *diagnosticInfo = nullptr,
                bool collectDiagnostics = false,
                SemanticProgram *semanticProgramOut = nullptr,
                const SemanticProductBuildConfig *semanticProductBuildConfig = nullptr) const;

  bool validateForBenchmark(
      Program &program,
      const std::string &entryPath,
      std::string &error,
      const std::vector<std::string> &defaultEffects,
      const std::vector<std::string> &entryDefaultEffects,
      const std::vector<std::string> &semanticTransforms = {},
      SemanticDiagnosticInfo *diagnosticInfo = nullptr,
      bool collectDiagnostics = false,
      SemanticProgram *semanticProgramOut = nullptr,
      const SemanticProductBuildConfig *semanticProductBuildConfig = nullptr,
      const SemanticValidationBenchmarkConfig &benchmarkConfig = {},
      const SemanticValidationBenchmarkObserver &benchmarkObserver = {}) const;
};

} // namespace primec
