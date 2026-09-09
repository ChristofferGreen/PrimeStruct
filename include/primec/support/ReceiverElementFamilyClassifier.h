#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace primec {

// Step 1a of docs/ReceiverTargetResolutionConsolidation.md.
//
// SCOPE, READ BEFORE WIRING THIS INTO A CALL SITE: this classifies a
// receiver/args-pack element's normalized type text into a dispatch family
// *in isolation*. It is deliberately NOT yet a drop-in replacement for any
// existing resolver. Auditing resolveArgsPackElementMethodTarget (the
// semantics-side implementation this module extracts name sets from)
// surfaced two method-name- and shape-gated quirks this classifier does not
// model:
//   - the FileError branch there only commits to the FileError family when
//     normalizedMethodName is one of a fixed 4-name set; for any other
//     method name on a FileError-typed element it falls through to the
//     struct-type-path fallback instead, so "family" alone is not what that
//     call site branches on - (type, methodName) jointly is.
//   - the vector/array/soa/Buffer/key-value/File checks there only run when
//     the element type text is template-shaped (`X<...>`, per that stage's
//     own splitTemplateTypeName); a bare non-template "Buffer" or "File"
//     element type skips them entirely and falls through to the struct
//     fallback too.
// Those quirks (real or latent bugs - undetermined) belong in the Step 0
// rule table before any call site is migrated onto this classifier's
// verdict; see the design doc's Open Questions section. Until then, treat
// this module as a verified, reusable *name-set library* (the vector/array
// base names, Buffer/File method-name sets, and primitive-name set that
// resolveArgsPackElementMethodTarget, resolveMethodCallTemplateTarget, and
// the ir_lowerer receiver-target helpers each currently re-type from
// scratch), not as a decision function ready to replace any of them.
//
// Step 1b (see classifyReceiverElementFamilyJoint below) resolves both
// quirks by taking the joint (type, methodName, templateShape) inputs the
// Step 0 Rule Table's Row A entry documents (R2/R2b, R3-R6b, R7) - that is
// the function to use for any new call-site wiring. classifyReceiverElementFamily
// above stays exactly as landed in Step 1a (type-text-only, quirks
// unaddressed) so its existing unit tests keep pinning the pre-Step-1b
// approximation; it is not itself wired anywhere either.
//
// Step 2 (2026-09-08/09): classifyReceiverElementFamilyJoint now drives real
// production behavior, not just observation - resolveArgsPackElementMethodTarget
// (SemanticsValidatorMethodTargetArgsPackResolvers.cpp) delegates to it
// directly, its own inline R1-R9 cascade deleted (2026-09-08), and
// resolveMethodTarget's indexed-args-pack cascade (slice 2,
// SemanticsValidatorExprMethodTargetResolution.cpp) likewise delegates to
// it directly, its own inline cascade deleted (2026-09-09) - both Step 1b
// diff-audit harnesses are retired, superseded by the real migrations. See
// docs/ReceiverTargetResolutionConsolidation.md's "Step 2" and "Step 2,
// second migration" sections for the migration detail and zero-divergence
// proof, including a caller pitfall:
// ReceiverElementFamilyResult::normalizedElementBaseType is always derived
// from the joint input's unwrappedElementType, never rawElementBaseType -
// a caller building the Primitive branch's resolved path from that result
// field instead of its own raw/wrapped text would silently lose the R7
// wrapped-vs-unwrapped asymmetry.
//
// Two families (Soa, KeyValue) are struct-metadata-backed and legitimately
// resolved differently per stage (each stage has its own struct/definition
// maps), so their membership test is a stage-supplied predicate - the same
// pattern CollectionSpellingClassifier uses for CollectionDefinitionExistsFn.
enum class ReceiverElementFamily {
  String,
  FileError,
  VectorLike,       // vector / array (pure name set; Soa is a separate family)
  Soa,
  Buffer,
  KeyValue,
  File,
  Primitive,
  StructOrUnknown,  // struct-typed element, or unrecognized: caller resolves
                     // the element's own type path (stage-specific).
};

struct ReceiverElementFamilyResult {
  ReceiverElementFamily family = ReceiverElementFamily::StructOrUnknown;
  // For VectorLike: the collection base name ("vector" or "array"), so the
  // caller can build "/<base>/<method>". Empty for every other family.
  std::string collectionBaseName;
  // The normalized element type text with a leading "/" stripped, as used
  // by the Primitive and StructOrUnknown branches.
  std::string normalizedElementBaseType;
};

// Stage-supplied membership tests for the two struct-metadata-backed
// families. Both are required (a null/empty std::function is treated as
// "never matches", which changes behavior - callers must pass their real
// predicate, not omit it).
struct ReceiverElementFamilyPredicates {
  std::function<bool(std::string_view)> isInternalSoaCollectionTypeName;
  std::function<bool(std::string_view)> isKeyValueSurfaceTypeName;
};

// `normalizedElementTypeText` must already be normalized (e.g. via the
// caller's normalizeBindingTypeName) and pointer/reference-unwrapped -
// this function does not unwrap `Reference<T>`/`Pointer<T>` itself, mirroring
// the call sites it consolidates (they unwrap before classifying).
ReceiverElementFamilyResult classifyReceiverElementFamily(
    std::string_view normalizedElementTypeText,
    const ReceiverElementFamilyPredicates &predicates);

// Pure name-set membership, exposed individually so callers (and the
// differential-audit harness) can pin/compare against the legacy
// per-stage literal lists directly.
bool isVectorLikeCollectionBaseName(std::string_view baseName);
bool isBufferAccessorMethodName(std::string_view methodName);
bool isFileHandleMethodName(std::string_view methodName);
bool isPrimitiveReceiverElementTypeName(std::string_view name);

// Step 1b of docs/ReceiverTargetResolutionConsolidation.md: the joint
// (type, methodName, templateShape) classifier that resolves the two
// quirks classifyReceiverElementFamily's header documents above, per the
// Step 0 Rule Table's Row category A (resolveArgsPackElementMethodTarget,
// SemanticsValidatorMethodTargetArgsPackResolvers.cpp:152-217) - branch
// order and gating replicated exactly, including the fall-through cases
// (R2b, R4b, R6b) that land on StructOrUnknown rather than a rejection.
//
// `unwrappedElementType` must be the Reference<T>/Pointer<T>-unwrapped
// element type text (== resolveArgsPackElementMethodTarget's own
// `collectionElemType`) - used for the String, FileError, and (when
// `isTemplateShaped`) the VectorLike/Soa/Buffer/KeyValue/File checks.
//
// `rawElementBaseType` is the *non*-unwrapped element type text, minus a
// leading '/' (== that function's own `normalizedElemBaseType`, computed
// from `elementTypeText` before the Reference/Pointer unwrap) - used only
// for the trailing Primitive check (R7). Production computes these two
// texts from different intermediate variables, so a Reference<i32>-typed
// element's String/FileError/VectorLike/... checks run against the
// unwrapped "i32" while its Primitive check runs against the *wrapped*
// "Reference<i32>" (not primitive) - this asymmetry is reproduced
// verbatim, not corrected, to stay byte-faithful to production; if this
// looks like a latent bug it is exactly the kind of finding Step 1b's
// diff harness exists to surface as a fresh Step 0 quirk, not something
// this classifier should silently "fix".
//
// `isTemplateShaped` and `templateShapedBaseName` must be the caller's own
// splitTemplateTypeName(unwrappedElementType) result (success flag and
// normalized base) - this classifier does not re-implement that parse
// (its own "matching '>' at the exact end of the string" requirement is
// stage-owned), it only gates on the caller's answer, per the Step 1a
// quirk writeup's "template-shape gating" finding.
struct ReceiverElementFamilyJointInput {
  std::string_view unwrappedElementType;
  std::string_view rawElementBaseType;
  bool isTemplateShaped = false;
  std::string_view templateShapedBaseName;
  std::string_view normalizedMethodName;
};

ReceiverElementFamilyResult classifyReceiverElementFamilyJoint(
    const ReceiverElementFamilyJointInput &input,
    const ReceiverElementFamilyPredicates &predicates);

// Step 1b, slice 2: wired at resolveMethodTarget's own inline indexed-
// args-pack-element cascade (SemanticsValidatorExprMethodTargetResolution.cpp,
// the `pack[i].method()` access shape - as opposed to
// resolveArgsPackElementMethodTarget's plain `pack_elem.method()` shape).
// That call site independently re-implements the *same* R1/R3-R7 family
// cascade classifyReceiverElementFamilyJoint already models, confirmed
// during Step 1b slice 2's audit to need no new branch or family - only a
// different relationship between its two text inputs:
//
//   - Its element type text is already Reference<T>/Pointer<T>-unwrapped
//     (via unwrapReferencePointerTypeText) *before* either the family
//     checks or the Primitive check run, so - unlike
//     resolveArgsPackElementMethodTarget's R7, which deliberately compares
//     the *non*-unwrapped raw text - this call site has no wrapped-vs-
//     unwrapped asymmetry at all: callers wiring this call site should pass
//     the same already-unwrapped text as both `unwrappedElementType` and
//     `rawElementBaseType`.
//   - Its FileError check is textually positioned *after* the template-
//     shape block instead of before it (opposite of R2's position in
//     resolveArgsPackElementMethodTarget). This is provably behavior-
//     preserving under this classifier's existing branch order: a
//     template-shaped type's parsed base name can never be the bare
//     literal "FileError" (that would require unparsed text like
//     "FileError<...>", which practice does not produce and neither call
//     site's cascade special-cases), so the two textual orderings are
//     mutually exclusive on any real input and this classifier's own
//     fixed R1/R2/R3-R6b/R7 ordering (FileError checked before the
//     template block) reproduces both call sites' verdicts identically.
//     Not re-derived as a new rule row; noted here so a future reader
//     does not mistake the reordering for an unmodeled divergence.
//
// See docs/ReceiverTargetResolutionConsolidation.md's Step 1b section for
// the wiring detail and zero-divergence proof.

// Env-gate for Step 1b's differential-audit harness
// (docs/ReceiverTargetResolutionConsolidation.md): when
// PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT is set (any value) in the
// environment, a wired call site additionally computes this classifier's
// verdict alongside its own existing inline answer and compares the two -
// purely for observation/logging, never substituting for the inline
// answer. Checked once and cached; unset by default, so default
// production behavior at every call site is unaffected.
bool isReceiverTargetDiffAuditEnabled();

// Human-readable family name for the diff-audit harness's log/assert
// messages. Deliberately NOT named "toString" - that name collides with
// doctest's ADL-based stringification hook and breaks CHECK(... ==
// ReceiverElementFamily::...) in any test file that has this header
// visible (discovered the hard way while extending this module's own unit
// tests).
const char *describeReceiverElementFamily(ReceiverElementFamily family);

} // namespace primec
