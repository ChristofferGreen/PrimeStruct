#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "primec/support/TextFilterPipeline.h"

namespace primec {
enum class DebugJsonSnapshotMode { None, Stop, All };

// IR optimization controls (docs/OptimizingBackendsPlan.md). Parsed from
// -O0..-O3 and --opt-* flags; nothing consumes them until the IR optimizer
// exists (TODO-5424), so every combination currently behaves like -O0.
struct OptimizationOptions {
  // 0..3; the value of the last -O<n> flag on the command line.
  uint8_t level = 0;
  // True when a -O<n> flag was given, so a later default flip can tell an
  // explicit -O0 from no flag at all.
  bool levelSpecified = false;
  // Pass names in command-line order. Names are checked against the pass
  // manifest by the optimizer, not by the parser.
  std::vector<std::string> enabledPasses;
  std::vector<std::string> disabledPasses;
  bool verifyEachPass = false;
  bool report = false;
};

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
  // When set, the pipeline compiles this text instead of reading `inputPath`;
  // `inputPath` then only names the primary unit (diagnostics, relative imports).
  std::optional<std::string> inMemorySource;
  std::string outputPath;
  std::string outDir = ".";
  std::string entryPath = "/main";
  bool inlineIrCalls = false;
  OptimizationOptions optimization;
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
  // TODO-4699 (docs/todo.md): reachability instrumentation for the legacy
  // hardcoded-3-slot vector-or-soa struct-layout branches and legacy
  // collection-vector method-call-resolution branches TODO-4700/TODO-4701
  // are evaluating for deletion.
  bool benchmarkIrLowererLegacyCollectionBranchCounters = false;
  // TODO-5226/TODO-5228 (docs/LibrarySymbolManifestLazyImports.md):
  // iterative lazy stdlib import expansion. When set, a wildcard/module-root
  // import of a module with a sibling `.psmeta` symbol manifest is spliced
  // in symbol-by-symbol on demand instead of as a whole file. Default-on as
  // of TODO-5226 (differential harness confirmed zero unintended divergence
  // across the full test corpus in TODO-5229); --whole-file-stdlib-imports
  // sets this back to false as an escape hatch, kept available for at least
  // one full session/release before considering removal.
  bool experimentalLazyStdlibImports = true;
};
} // namespace primec
