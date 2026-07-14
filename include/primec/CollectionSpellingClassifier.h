#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace primec {

// Single authoritative decision for compat/legacy collection-helper
// spellings, per docs/CompatPathResolutionConsolidation.md. Every compiler
// stage should consult this instead of composing the spelling primitives
// itself. Dispositions:
// - PassThrough: not a spelling this rule owns, or intentionally left
//   unresolved so a same-path shadow definition can match or generic
//   unknown-target handling can reject it (pass-through is load-bearing;
//   see the doc's Step 0 findings).
// - Canonicalize: rewrite to canonicalPath. Only ever produced when the
//   canonical target definition exists (decision D5: never emit a path
//   without a backing definition).
// - Reject: retired spelling; the call site should emit its
//   unknown-target diagnostic for this shape (wording stays per-site for
//   now, decision D4).
enum class CompatSpellingDisposition {
  PassThrough,
  Canonicalize,
  Reject,
};

enum class CollectionCallShape {
  DirectCall,
  MethodCall,
  BindingInitializer,
};

enum class CollectionReceiverFamily {
  None,
  Vector,
  Soa,
  Map,
};

struct CompatSpellingDecision {
  CompatSpellingDisposition disposition = CompatSpellingDisposition::PassThrough;
  std::string canonicalPath;
  std::string_view rejectReason;
};

// Stage-supplied definition lookup. Each stage legitimately has a
// different definition map (monomorphization's ctx.sourceDefs +
// ctx.helperOverloads, the validator's defMap_, ir_lowerer's defMap); the
// shadow-precedence and canonical-target-exists rules are shared even
// though the map is not.
using CollectionDefinitionExistsFn = std::function<bool(std::string_view path)>;

CompatSpellingDecision classifyCollectionHelperSpelling(
    std::string_view path,
    CollectionCallShape shape,
    CollectionReceiverFamily receiverFamily,
    const CollectionDefinitionExistsFn &definitionExists);

// Decision D2: the single authoritative retired/removed name sets. The
// scattered hardcoded copies and registry-membership variants elsewhere
// migrate onto these; a unit test asserts these agree with the registry
// today.
bool classifierRemovedVectorCompatibilityHelper(std::string_view helperName);
bool classifierRemovedBorrowedSoaCompatibilityHelper(std::string_view helperName);
bool classifierRemovedKeyValueCompatibilityHelper(std::string_view helperName);
// Mechanism B (Step 0 findings): helpers whose method-call form on an SOA
// receiver is retired and must reject, independent of the isRemoved* sets.
bool classifierRetiredSoaReceiverMethodHelper(std::string_view helperName);
// The public-surface SOA helper names eligible for bare-path
// canonicalization (decision D1).
bool classifierPublicSoaSurfaceHelperName(std::string_view helperName);

} // namespace primec
