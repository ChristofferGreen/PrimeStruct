#pragma once

// TODO-4699 (docs/todo.md): benchmark-flag-gated reachability instrumentation
// for the legacy hardcoded-3-slot vector-or-soa struct-layout branches and the
// legacy collection-vector method-call-resolution branches that TODO-4700/
// TODO-4701 are evaluating for deletion. Enabled via
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
  // IrLowererStructSlotLayoutHelpers.cpp: the two remaining hardcoded
  // 3-slot early-exit branches inside the per-field enumeration loop of
  // resolveStructSlotLayoutFromDefinitionFields (isBuiltinVectorTypeName /
  // isBuiltinSoaVectorTypeName), distinct from the two early-exit branches
  // TODO-4670 already removed from the top-level struct-path case.
  uint64_t structSlotLayoutVectorBranchHits = 0;
  uint64_t structSlotLayoutSoaBranchHits = 0;

  // IrLowererUninitializedStructInference.cpp:normalizeUninitializedVectorStructPath
  // - independent duplicate definition of isBuiltinVectorTypeName's check.
  uint64_t uninitializedStructInferenceDuplicateHits = 0;

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
  uint64_t collectionVectorOwnerPathTargetPathFallbackNullptrHits = 0;
  uint64_t collectionVectorOwnerPathReceiverTypeSiteHits = 0;

  // Count of logged divergences from the dual-computation equivalence check
  // (hardcoded 3-slot layout vs. what the generic field-based layout path
  // would have produced for the same input).
  uint64_t structSlotLayoutDivergenceCount = 0;
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
void recordLegacyCollectionBranchHitStructSlotLayoutVector();
void recordLegacyCollectionBranchHitStructSlotLayoutSoa();
void recordLegacyCollectionBranchHitUninitializedStructInferenceDuplicate();
void recordLegacyCollectionBranchHitCollectionVectorMetadataMethodPath();
void recordLegacyCollectionBranchHitCollectionVectorOwnerPath();

// TODO-4701: finer-grained recorders for the two isCollectionVectorOwnerPath
// call sites (see the struct fields above for what each counts). Each of
// these is recorded in addition to (not instead of) the coarse
// recordLegacyCollectionBranchHitCollectionVectorOwnerPath() call already at
// both sites.
void recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathSite();
void recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathFallbackResolved();
void recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathFallbackNullptr();
void recordLegacyCollectionBranchHitCollectionVectorOwnerPathReceiverTypeSite();

// Logs a single divergence (log-only, non-fatal) between the hardcoded
// 3-slot layout result actually used and what the generic field-based
// layout path computed (or failed to compute) for the same field. No-op
// unless legacyCollectionBranchCountersEnabled() is true.
void recordLegacyCollectionBranchStructSlotLayoutDivergence(
    const std::string &site,
    const std::string &hardcodedStructPath,
    int32_t hardcodedSlotCount,
    bool genericResolved,
    const std::string &genericStructPath,
    int32_t genericSlotCount);

// Emits the "[benchmark-ir-lowerer-legacy-collection-branch-counters]"
// summary line to stderr, in the same style as
// emitBenchmarkSemanticPhaseCounters in src/bin/main.cpp. No-op unless
// legacyCollectionBranchCountersEnabled() is true. Also registered
// automatically via std::atexit the first time counting is enabled, so a
// process that never calls this explicitly (e.g. a doctest unit-test
// binary) still reports on exit.
void emitLegacyCollectionBranchCountersReport();

} // namespace primec::ir_lowerer
