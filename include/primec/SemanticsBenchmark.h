#pragma once

#include "primec/Semantics.h"

#include <unordered_set>

namespace primec {

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

bool validateSemanticsForBenchmark(
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
    const SemanticValidationBenchmarkObserver &benchmarkObserver = {},
    const std::unordered_set<std::string> *lazyStdlibModuleKeys = nullptr);

} // namespace primec
