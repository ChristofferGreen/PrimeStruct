#pragma once

// Benchmark-flag-gated reachability instrumentation for the legacy
// collection-vector method-call-resolution branches. The struct-slot-layout and
// uninitialized-struct counters were deleted after a whole-suite run read zero. Enabled via
// --benchmark-ir-lowerer-legacy-collection-branch-counters (primec CLI,
// threaded through Options::benchmarkIrLowererLegacyCollectionBranchCounters)
// or via the PRIMEC_BENCHMARK_IR_LOWERER_LEGACY_COLLECTION_BRANCH_COUNTERS
// environment variable (used to flip the flag on for whole test-suite runs,
// e.g. unit-test binaries and compile_run subprocess invocations that do not
// go through primec's CLI option parsing at all, or that ctest launches with
// a fixed argv this task does not want to disturb). Purely additive: when
// disabled (the default) this instrumentation is a no-op and changes no
// behavior or return values anywhere it is called from.

#include <cstdint>
#include <string>

namespace primec::ir_lowerer {

struct LegacyCollectionBranchCounters {
  // IrLowererSetupTypeMethodCallResolution.cpp: isCollectionVectorOwnerPath /
  // isCollectionVectorMetadataMethodPath causing the caller to take the
  // legacy receiver-resolved-vector-metadata path instead of falling through
  // to the generic method-call target resolution below it.
  uint64_t collectionVectorMetadataMethodPathHits = 0;
  uint64_t collectionVectorOwnerPathHits = 0;

  // TODO-4701 (docs/todo.md): finer-grained split of
  // collectionVectorOwnerPathHits by which of the two isCollectionVectorOwnerPath
  // call sites fired, and, for the targetPath call site, whether the
  // tryResolvedPath(targetPath) fallback ahead of that branch's
  // `return nullptr` (landed 2026-07-04/05) actually resolved something or
  // fell through to nullptr. The two site counters sum to
  // collectionVectorOwnerPathHits; the two targetPath-site sub-counters sum
  // to collectionVectorOwnerPathTargetPathSiteHits.
  uint64_t collectionVectorOwnerPathTargetPathSiteHits = 0;
  uint64_t collectionVectorOwnerPathTargetPathFallbackResolvedHits = 0;
  uint64_t collectionVectorOwnerPathReceiverTypeSiteHits = 0;
};

// Enables or disables counter collection, the dual-computation equivalence
// check, and end-of-process reporting. Also consulted (OR'd in) is the
// PRIMEC_BENCHMARK_IR_LOWERER_LEGACY_COLLECTION_BRANCH_COUNTERS environment
// variable, checked once at first use, so a whole test-suite invocation can
// enable this instrumentation without needing to alter every individual
// primec/test-binary invocation's argv.
void setLegacyCollectionBranchCountersEnabled(bool enabled);
bool legacyCollectionBranchCountersEnabled();

// Reserved for callers (e.g. tests) that want a clean slate; not used by
// the primec CLI path itself since a process only runs one compile.
void resetLegacyCollectionBranchCounters();

const LegacyCollectionBranchCounters &legacyCollectionBranchCounters();

// Recording entry points. Each is a no-op unless
// legacyCollectionBranchCountersEnabled() is true.
void recordLegacyCollectionBranchHitCollectionVectorMetadataMethodPath();
void recordLegacyCollectionBranchHitCollectionVectorOwnerPath();

// Finer-grained recorders for the two isCollectionVectorOwnerPath
// call sites (see the struct fields above for what each counts). Each of
// these is recorded in addition to (not instead of) the coarse
// recordLegacyCollectionBranchHitCollectionVectorOwnerPath() call already at
// both sites.
void recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathSite();
void recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathFallbackResolved();
void recordLegacyCollectionBranchHitCollectionVectorOwnerPathReceiverTypeSite();

// Emits the "[benchmark-ir-lowerer-legacy-collection-branch-counters]"
// summary line to stderr, in the same style as
// emitBenchmarkSemanticPhaseCounters in src/bin/main.cpp. No-op unless
// legacyCollectionBranchCountersEnabled() is true. Also registered
// automatically via std::atexit the first time counting is enabled, so a
// process that never calls this explicitly (e.g. a doctest unit-test
// binary) still reports on exit.
void emitLegacyCollectionBranchCountersReport();

} // namespace primec::ir_lowerer
