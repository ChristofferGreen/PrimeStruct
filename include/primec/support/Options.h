#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "primec/support/TextFilterPipeline.h"

namespace primec {
enum class DebugJsonSnapshotMode { None, Stop, All };

// IR optimization controls (docs/OptimizingBackendsPlan.md). Parsed from
// -O0..-O3 and --opt-* flags; optimizeIrModule (primec/ir/IrOptimizer.h) reads
// them. With no -O flag, parseOptions picks -O2 for primevm and for the VM,
// native and optexe/optcpp outputs, and -O0 for everything else (dumps, debug
// sessions, serialized IR, wasm, GLSL, C++).
struct OptimizationOptions {
  // 0..3; the value of the last -O<n> flag on the command line, or the default
  // parseOptions picked for this output.
  uint8_t level = 0;
  // True when a -O<n> flag was given, to tell an explicit -O0 from the default.
  bool levelSpecified = false;
  // Pass names in command-line order. Names are checked against the pass
  // manifest by the optimizer, not by the parser.
  std::vector<std::string> enabledPasses;
  std::vector<std::string> disabledPasses;
  bool verifyEachPass = false;
  bool report = false;
};

// --profile: the named set of defaults a run starts from. Program is today's behavior; Build is the sandbox of a
// build script (docs/spec/integration-and-tooling.md, Run profiles).
enum class RunProfile { Program, Build };

struct Options {
  std::string emitKind;
  std::string wasmProfile = "wasi";
  bool listTransforms = false;
  bool listOptimizationPasses = false;
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
  // `--emit=lib --with-sources`: the archive also holds the module's `.prime` files.
  bool libraryWithSources = false;
  // `--library <name>.psa` (repeatable): the library archives the program is compiled against and linked with
  // (docs/spec/integration-and-tooling.md, Compiling against a library).
  std::vector<std::string> libraryPaths;
  OptimizationOptions optimization;
  std::string dumpStage;
  std::vector<std::string> textFilters = {"collections", "operators", "implicit-utf8", "implicit-i32"};
  std::vector<TextTransformRule> textTransformRules;
  std::vector<TextTransformRule> semanticTransformRules;
  bool allowEnvelopeTextTransforms = true;
  std::vector<std::string> semanticTransforms = {"single_type_to_return"};
  bool requireCanonicalSyntax = false;
  // --profile: a flag given explicitly always overrides the profile's value, whichever comes first.
  RunProfile profile = RunProfile::Program;
  std::vector<std::string> defaultEffects = {"io_out"};
  std::vector<std::string> entryDefaultEffects = {"io_out"};
  // --allow-effects: the effects user code may declare (stdlib paths are not checked); unset is no ceiling.
  std::optional<std::vector<std::string>> allowEffects;
  // --allow-import: the stdlib import prefixes user source may name; unset is no restriction, empty allows none.
  std::optional<std::vector<std::string>> allowImport;
  // --emit-deps: write the sorted source files the compile read, one path per line, to this file.
  std::string emitDepsPath;
  std::vector<std::string> programArgs;
  std::vector<std::string> importPaths;
  // --link-arg: extra arguments for the host C++ compiler when it links an exe or optexe output,
  // such as a host library the program's `[host]` functions come from.
  std::vector<std::string> linkArgs;
  // primevm --link <module.psir> (repeatable): serialized IR modules merged into the program before it runs
  // (primec/ir/IrLinker.h); the program is a source file or a serialized module (a `.psir` input).
  std::vector<std::string> linkModulePaths;
  // --vm-schedule-seed (or PRIMESTRUCT_VM_SCHEDULE_SEED): the VM's task schedule; none is the eager default
  // (docs/spec/vm-design.md, Tasks).
  std::optional<uint64_t> vmScheduleSeed;
  // --vm-threads: the VM runs each task on an OS thread of its own (docs/spec/vm-design.md, Tasks on threads); the
  // schedule seed does not apply then. Off by default.
  bool vmThreads = false;
  // --vm-record-sync <path> (needs --vm-threads) writes the order of the run's synchronization events;
  // --vm-replay-sync <path> runs the program on the deterministic scheduler following such a record, and ignores a
  // schedule seed (docs/spec/vm-design.md, Synchronization record and replay). The two are exclusive.
  std::string vmRecordSyncPath;
  std::string vmReplaySyncPath;
  // --vm-repeat <n> (primevm only, needs --vm-threads) compiles the program once and runs the module n times, each
  // run on a fresh VM; the runs' output follows one another and the exit code is the runs' common one (the
  // concurrent corpus, tests/vm_threads_corpus.txt, pays the front end once per program with it). 0 = one run.
  uint32_t vmRepeat = 0;
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
