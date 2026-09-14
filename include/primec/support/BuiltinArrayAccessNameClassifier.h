#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace primec {

// TODO-5293 (docs/ReceiverTargetResolutionConsolidation.md): shared-logic
// extraction for `getBuiltinArrayAccessName`, which today is implemented
// twice - once per stage (`semantics::getBuiltinArrayAccessName`,
// `SemanticsBuiltinPathHelpers.cpp:1186`, and
// `ir_lowerer::getBuiltinArrayAccessName`, `IrLowererBuiltinNameHelpers.cpp:505`)
// - with five characterized branch-level divergences between them (see the
// design doc's "TODO-5293 Step (1)/(2)/(3)" sections for the full audit
// history). This module follows the `ReceiverElementFamilyClassifier`/
// `CanonicalReceiverType` extraction pattern from TODO-5294: pure,
// stage-agnostic PRIMITIVES for the low-level matching logic that is
// genuinely identical between the two stages, one stage-supplied callback
// for the one part that is genuinely different in shape (branch 5, the
// key-value-helper delegate), and a per-stage COMPOSITION function
// (`classifyBuiltinArrayAccessNameForSemantics`/`...ForIrLowerer`) that
// reproduces that stage's real root-walking sequence from the shared
// primitives - because, per the audit history below, the two stages' real
// bodies do not walk the same roots in the same order with the same
// hard-stop shape, a single joint "one function both stages call
// unmodified" entry point would not be a faithful reproduction of either.
//
// SCOPE, READ BEFORE WIRING THIS INTO A PRODUCTION CALL SITE: as of this
// round, this module is NOT wired into either stage's production
// `getBuiltinArrayAccessName` - only into an observational diff-audit
// harness (gated by `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT`, mirroring the
// pattern TODO-5294 used throughout) at the semantics-stage call site. See
// the design doc for which stage(s) were actually harnessed this round,
// with real test-suite traffic, and the zero-divergence proof.
//
// How the five previously-characterized branches map onto this design:
//
//   Branch 1 (`Expr::Kind::Call` gate, ir_lowerer-only): NOT encoded inside
//   this module. Per TODO-5293 Step (2)'s finding, the gate is practically
//   inert everywhere it matters - `Expr::name` is only ever populated on
//   `Call`-/`Name`-kind nodes anywhere in the codebase, so a non-`Call`
//   expr reaching either stage's real function already fails on
//   `name.empty()`, or (for `Name`-kind, the one real gap site,
//   `resolveBuiltinKeyValueInsertReceiverBinding`,
//   `SemanticsValidate.cpp:614-681`) is excluded by that site's own
//   earlier `Kind::Name` check. This module operates on caller-supplied
//   strings, not `Expr`, so there is no `Kind` to gate on here regardless;
//   ir_lowerer's real production function already has an explicit
//   `Kind::Call` check ahead of everything this module reproduces, and
//   that check is left entirely to the production call site, not
//   duplicated here.
//
//   Branch 2 (bare capitalized "At"/"AtUnsafe", semantics-only): already
//   deleted from production (TODO-5293 Step (3)) as confirmed dead code.
//   `classifyAccessAliasToken` below correspondingly does NOT recognize a
//   bare "At"/"AtUnsafe" spelling in any spelling mode; only lowercase
//   "at"/"at_ref"/"at_unsafe"/"at_unsafe_ref" and the concatenated
//   "vectorAt"/"vectorAtUnsafe" forms are ever recognized, matching both
//   stages' real (post-deletion) behavior.
//
//   Branches 3 and 4 (vector-receiver-base disambiguation, internal-SOA-
//   storage-column - both ir_lowerer-only, confirmed by TODO-5293 Step (2)
//   to be constructed only inside ir_lowerer's own lowering machinery with
//   no construction site semantics can reach): NOT part of the shared
//   PRIMITIVES' fixed behavior - `matchBuiltinArrayAccessAliasUnderPrefix`
//   only performs the disambiguation check when its caller supplies a
//   non-empty `receiverBase`, and `classifyBuiltinArrayAccessNameForSemantics`
//   never does. They ARE reproduced inside
//   `classifyBuiltinArrayAccessNameForIrLowerer` (branch 3 as two
//   `receiverBase`-bearing primitive calls; branch 4 as one more such call
//   plus a stage-supplied fallback callback for
//   `normalizeInternalSoaStorageBuiltinAlias`, which this module does not
//   reimplement) - exactly the "ir_lowerer's own call site keeps a small
//   amount of wrapper logic around the shared call" shape this task asked
//   for, realized as ir_lowerer's dedicated composition function calling
//   the same shared primitive semantics' composition function also calls,
//   just with arguments semantics' composition never passes.
//
//   Branch 5 (key-value-helper delegate - confirmed genuinely different
//   per-stage, not just differently typed, by a fresh reading of both real
//   bodies this round): carried as the `BuiltinArrayAccessKeyValueLookup`
//   callback below. Each stage supplies its OWN real implementation:
//   semantics' `resolveKeyValueHelperMemberNameLocal` resolves a member-
//   name string (cross-checking the resolved path's owning surface-
//   metadata id) and then classifies THAT resolved name through
//   `classifyAccessAliasToken` - so a key-value path CAN yield
//   `"at"`/`"at_unsafe"` (`kAccept`). ir_lowerer's real body instead uses
//   the bool-only, no-cross-check `resolvesKeyValueHelperSurfacePath` as
//   an unconditional REJECTION of any key-value-surface match - true means
//   "definitely not an array-access name" (`kReject`), it never accepts.
//   The callback's `BuiltinArrayAccessAliasOutcome` return shape is
//   expressive enough to carry both of these real behaviors without
//   forcing one stage's shape onto the other.

// The literal member-name spellings `classifyAccessAliasToken` compares
// against differ per real call site within EACH stage, not just between
// stages: ir_lowerer's own `matchAccessAlias` lambda (branch-3/4's
// receiver-base-disambiguation shape) only ever compares against the bare
// spellings ("at"/"at_ref"/"at_unsafe"/"at_unsafe_ref"), while its sibling
// `matchLegacyAccessAlias` lambda (the plain-prefix shape) only ever
// compares against the concatenated spellings ("vectorAt"/"vectorAtUnsafe")
// - the two lambdas are never interchangeable, and neither alone covers
// what semantics' single `accessAliasFromMemberName` lambda covers (all
// six spellings, verified as the union of ir_lowerer's two subsets). This
// enum selects which literal set a given call compares against, so the
// underlying strip-suffix-then-compare pipeline can stay one shared
// function instead of being re-typed per subset.
enum class AccessAliasSpellingMode {
  kBareOnly,          // "at"/"at_ref" -> "at"; "at_unsafe"/"at_unsafe_ref" -> "at_unsafe"
  kConcatenatedOnly,  // "vectorAt" -> "at"; "vectorAtUnsafe" -> "at_unsafe"
  kFull,              // the union of both of the above (semantics' real shape)
};

// The three things a decision point in either stage's real
// `getBuiltinArrayAccessName` body can do at a given prefix/root: give up
// on this root and let the caller try the next one (`kNoMatch`); commit to
// "this is definitely not an array-access name" right here, with no
// further roots or fallback tried (`kReject` - the "hard stop" shape both
// stages' real bodies use once a name is known to be *under* a given root
// but doesn't classify as `at`/`at_unsafe`); or commit to an accepted
// `"at"`/`"at_unsafe"` token (`kAccept`).
enum class BuiltinArrayAccessAliasOutcome {
  kNoMatch,
  kReject,
  kAccept,
};

struct BuiltinArrayAccessAliasResult {
  BuiltinArrayAccessAliasOutcome outcome = BuiltinArrayAccessAliasOutcome::kNoMatch;
  // Set only when outcome == kAccept; always "at" or "at_unsafe".
  std::string token;
};

// Stage-supplied lookup for branch 5 (the key-value-helper delegate). Given
// the (already stage-normalized) scoped name, report:
//   - kNoMatch if `name` does not resolve against this stage's key-value/
//     map stdlib surface at all (the caller keeps trying other roots/the
//     raw-name fallback);
//   - kReject if `name` resolves against that surface but this stage's own
//     real logic does not accept it as an array-access alias here (this is
//     ir_lowerer's real shape: any key-value-surface match is a hard "not
//     array access", regardless of which member it resolved to);
//   - kAccept("at"/"at_unsafe") if `name` resolves against that surface AND
//     this stage's own real logic classifies the resolved member as an
//     access alias (semantics' real shape).
using BuiltinArrayAccessKeyValueLookup =
    std::function<BuiltinArrayAccessAliasResult(std::string_view scopedName)>;

// Pure, 100%-shared-between-stages primitive: normalizes `memberName`
// (strips a `__t<hash>` template-specialization suffix, then a `__<n>`
// generated-name suffix - exactly the two-step
// `stripTemplateSpecializationSuffix`/`stripGeneratedSuffix` pipeline both
// stages' real lambdas already apply, verified byte-identical formulas
// across both stages) and classifies the result against `mode`'s literal
// set (see `AccessAliasSpellingMode` above). Returns the normalized
// "at"/"at_unsafe" token, or nullopt if `memberName` does not match.
// Deliberately does NOT recognize the bare capitalized "At"/"AtUnsafe"
// spelling in any mode - that branch was confirmed dead and deleted from
// semantics' production code in TODO-5293 Step (3).
std::optional<std::string> classifyAccessAliasToken(std::string memberName,
                                                     AccessAliasSpellingMode mode);

// Pure, 100%-shared-between-stages primitive generalizing the "strip a
// known root prefix off `name`, optionally require a receiver-base
// disambiguation segment, then classify the remaining alias" shape both
// stages' real bodies use repeatedly.
//
// If `name` does not start with `prefix`, returns kNoMatch. Otherwise the
// prefix is considered "present" and the function always returns kReject
// or kAccept (never kNoMatch again) - callers that need to know "was this
// name under this root at all, regardless of whether it classified" (the
// hard-stop shape both stages' real bodies use, and branches 3/4's own
// wrapper logic needs) read that off `outcome != kNoMatch`; semantics'
// composition below never needs that distinction, since its own multi-root
// checks are a plain boolean OR-chain that only cares about kAccept.
//
// If `receiverBase` is empty (the "legacy", no-embedded-receiver shape):
// the alias is the remainder after stripping `prefix`; if that remainder
// still contains a '/', the match is rejected outright (both stages' real
// code either checks this explicitly or gets the same net result for
// free, since none of the six recognized spellings contain '/').
//
// If `receiverBase` is non-empty (branch 3's shape, ir_lowerer's
// `matchAccessAlias`): when the remainder contains a '/', the segment
// before that '/' must equal `receiverBase` or start with
// `receiverBase + "__"` (a specialized/monomorphized receiver type name)
// or the match is rejected - the disambiguation check itself, guarding
// against some *other* receiver type's member colliding by spelling under
// the same folder. When the remainder has no '/' at all, there is nothing
// to disambiguate and the whole remainder is classified directly.
//
// `rejectOnRawResidualSlash` (only consulted when `receiverBase` is empty)
// selects between the two real "legacy" (no-receiver-base) shapes found
// across both stages' bodies: true (the default, and every real call site
// except one) rejects immediately if the RAW alias - before any suffix
// stripping - still contains '/', matching semantics'
// `matchStdlibLegacyAccessAlias` and ir_lowerer's `matchLegacyAccessAlias`
// exactly; false skips that pre-check and instead strips suffixes first,
// relying on the post-strip literal compare to naturally reject a residual
// slash - matching semantics' inline `stdVectorRoot` handling, the one
// real call site where a '/' occurring after a `__t<hash>` marker gets
// stripped away before classification is attempted (so, for a contrived
// alias like "at__t9/foo", the two settings genuinely disagree: true
// rejects outright, false strips to "at" and accepts).
BuiltinArrayAccessAliasResult matchBuiltinArrayAccessAliasUnderPrefix(
    std::string_view name, std::string_view prefix, std::string_view receiverBase,
    AccessAliasSpellingMode mode, bool rejectOnRawResidualSlash = true);

// Semantics-stage composition: reproduces `semantics::getBuiltinArrayAccessName`
// bit-for-bit (post-branch-2-deletion) using the shared primitives above
// plus a stage-supplied key-value lookup for branch 5. `name` must already
// be normalized exactly as semantics' real function normalizes it
// (namespacePrefix folded in when `name` itself has no '/', leading '/'
// stripped) - a few lines of `Expr`-specific glue this module deliberately
// leaves to the caller (see the header's top comment on why `Expr` itself
// is not a dependency here). `rawName` is the unnormalized `expr.name`
// (leading-'/'-stripped), used only by the final raw-name fallback exactly
// as semantics' real function uses it. `stdCollectionsPrefix`,
// `experimentalVectorPrefix`, `experimentalMapPrefix` and
// `stdVectorRootPrefix` are the four root prefixes semantics' real
// function computes via `collectionMemberRootLocal`/
// `experimentalCollectionMemberRootLocal` - passed in rather than
// recomputed here so this module stays free of a `StdlibCollectionPaths`
// dependency.
bool classifyBuiltinArrayAccessNameForSemantics(
    const std::string &name, const std::string &rawName, const std::string &stdCollectionsPrefix,
    const std::string &experimentalVectorPrefix, const std::string &experimentalMapPrefix,
    const std::string &stdVectorRootPrefix,
    const BuiltinArrayAccessKeyValueLookup &resolveKeyValueHelper, std::string &out);

// ir_lowerer-stage composition: reproduces `ir_lowerer::getBuiltinArrayAccessName`
// bit-for-bit using the shared primitives above - including branches 3 and
// 4 (vector-receiver-base disambiguation, internal-SOA-storage-column),
// both realized here as ir_lowerer-specific calls into
// `matchBuiltinArrayAccessAliasUnderPrefix` with a non-empty `receiverBase`
// that semantics' composition above never passes - plus a stage-supplied
// key-value lookup for branch 5. `scopedName` must already be normalized
// exactly as ir_lowerer's real function normalizes it
// (`resolveScopedExprName`, then leading-'/' stripped); `rawName` is
// `expr.name` (leading-'/'-stripped). The five prefix arguments and two
// vector-helper-path arguments mirror what ir_lowerer's real function
// computes via `collectionMemberRoot`/`experimentalCollectionMemberRoot`/
// `collection_paths::modulePrefixBare`/`unrootedStdlibVectorHelperPath` -
// passed in rather than recomputed here for the same dependency-isolation
// reason as the semantics composition above.
// `resolveInternalSoaStorageFallbackAlias` mirrors the real body's second,
// distinct SOA-column fallback (`normalizeInternalSoaStorageBuiltinAlias`)
// - it is invoked only when `scopedName` is under
// `internalSoaStorageFolderPrefix` but the receiver-base-disambiguated
// match already failed; may be an empty `std::function` when the caller
// does not need that fallback (e.g. unit tests exercising other roots).
bool classifyBuiltinArrayAccessNameForIrLowerer(
    const std::string &scopedName, const std::string &rawName, const std::string &stdCollectionsPrefix,
    const std::string &stdVectorRootPrefix, const std::string &experimentalVectorRootPrefix,
    const std::string &legacyVectorFolderPrefix, const std::string &internalSoaStorageFolderPrefix,
    const std::string &vectorHelperPathAtName, const std::string &vectorHelperPathAtUnsafeName,
    const BuiltinArrayAccessKeyValueLookup &resolveKeyValueHelper,
    const std::function<std::optional<std::string>(const std::string &)> &resolveInternalSoaStorageFallbackAlias,
    std::string &out);

}  // namespace primec
