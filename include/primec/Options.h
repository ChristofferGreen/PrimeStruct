#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "primec/TextFilterPipeline.h"

namespace primec {
enum class DebugJsonSnapshotMode { None, Stop, All };

struct Options {
  std::string emitKind;
  std::string wasmProfile = "wasi";
  bool listTransforms = false;
  bool emitDiagnostics = false;
  bool debugJson = false;
  bool debugDap = false;
  std::string debugTracePath;
  std::string debugReplayPath;
  std::optional<uint64_t> debugReplaySequence;
  DebugJsonSnapshotMode debugJsonSnapshotMode = DebugJsonSnapshotMode::None;
  bool collectDiagnostics = false;
  std::string inputPath;
  std::string outputPath;
  std::string outDir = ".";
  std::string entryPath = "/main";
  bool inlineIrCalls = false;
  std::string dumpStage;
  std::vector<std::string> textFilters = {"collections", "operators", "implicit-utf8", "implicit-i32"};
  std::vector<TextTransformRule> textTransformRules;
  std::vector<TextTransformRule> semanticTransformRules;
  bool allowEnvelopeTextTransforms = true;
  std::vector<std::string> semanticTransforms = {"single_type_to_return"};
  bool requireCanonicalSyntax = false;
  std::vector<std::string> defaultEffects = {"io_out"};
  std::vector<std::string> entryDefaultEffects = {"io_out"};
  std::vector<std::string> programArgs;
  std::vector<std::string> importPaths;
  bool skipSemanticProductForNonConsumingPath = false;
  std::optional<bool> benchmarkForceSemanticProduct;
  bool benchmarkSemanticNoFactEmission = false;
  bool benchmarkSemanticFactFamiliesSpecified = false;
  std::vector<std::string> benchmarkSemanticFactFamilies;
  bool benchmarkSemanticTwoChunkDefinitionValidation = false;
  std::optional<uint32_t> benchmarkSemanticDefinitionValidationWorkerCount;
  bool benchmarkSemanticPhaseCounters = false;
  bool benchmarkSemanticAllocationCounters = false;
  bool benchmarkSemanticRssCheckpoints = false;
  bool benchmarkSemanticDisableMethodTargetMemoization = false;
  bool benchmarkSemanticGraphLocalAutoLegacyKeyShadow = false;
  bool benchmarkSemanticGraphLocalAutoLegacySideChannelShadow = false;
  bool benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr = false;
  std::optional<uint32_t> benchmarkSemanticRepeatCompileCount;
  // TODO-5228 (docs/LibrarySymbolManifestLazyImports.md): opt-in iterative
  // lazy stdlib import expansion. When set, a wildcard/module-root import
  // of a module with a sibling `.psmeta` symbol manifest is spliced in
  // symbol-by-symbol on demand instead of as a whole file. Default-off;
  // today's whole-file-splice behavior is unchanged when this is false.
  bool experimentalLazyStdlibImports = false;
};
} // namespace primec
