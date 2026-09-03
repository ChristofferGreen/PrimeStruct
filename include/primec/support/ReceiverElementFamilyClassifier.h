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

} // namespace primec
