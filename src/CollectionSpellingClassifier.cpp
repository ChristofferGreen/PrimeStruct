#include "primec/CollectionSpellingClassifier.h"

#include "primec/StdlibSurfaceRegistry.h"

#include <array>

namespace primec {

namespace {

constexpr std::string_view kArrayCompatPrefix = "/array/";
constexpr std::string_view kRootedVectorPrefix = "/vector/";
constexpr std::string_view kCanonicalVectorPrefix = "/std/collections/vector/";
constexpr std::string_view kRootedMapPrefix = "/map/";
constexpr std::string_view kBareSoaPrefix = "/soa/";
constexpr std::string_view kCanonicalSoaPrefix = "/std/collections/soa/";
constexpr std::string_view kDeadSoaVectorPrefix = "/soa_vector/";
constexpr std::string_view kDeadCanonicalSoaVectorPrefix =
    "/std/collections/soa_vector/";

constexpr std::string_view kRejectRetiredSoaMethod = "retired-soa-method";
constexpr std::string_view kRejectRemovedVectorMethod = "removed-vector-method";
constexpr std::string_view kRejectRemovedKeyValueMethod =
    "removed-key-value-method";

bool startsWith(std::string_view value, std::string_view prefix) {
  return value.substr(0, prefix.size()) == prefix;
}

std::string_view leafName(std::string_view path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string canonicalVectorHelperPath(std::string_view helperName) {
  return std::string(kCanonicalVectorPrefix) + std::string(helperName);
}

std::string canonicalSoaHelperPath(std::string_view helperName) {
  return std::string(kCanonicalSoaPrefix) + std::string(helperName);
}

} // namespace

bool classifierRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" ||
         helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool classifierRemovedBorrowedSoaCompatibilityHelper(std::string_view helperName) {
  return helperName == "count_ref" || helperName == "get_ref" ||
         helperName == "ref_ref" || helperName == "to_aos_ref";
}

bool classifierRemovedKeyValueCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "size" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

bool classifierRetiredSoaReceiverMethodHelper(std::string_view helperName) {
  // Mechanism B: retired method-call surface on SOA receivers. `ref`,
  // `push`, and `reserve` sit in no isRemoved* set; their rejection is
  // pinned by test_semantics_calls_and_flow_collections_container_error_
  // and_result_helpers.cpp:1691 and :1807. The borrowed *_ref helpers are
  // retired in method position as well.
  return helperName == "ref" || helperName == "push" ||
         helperName == "reserve" ||
         classifierRemovedBorrowedSoaCompatibilityHelper(helperName);
}

bool classifierPublicSoaSurfaceHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "get" || helperName == "get_ref" ||
         helperName == "ref" || helperName == "ref_ref" ||
         helperName == "soa" || helperName == "single" ||
         helperName == "from_aos" || helperName == "field_view";
}

CompatSpellingDecision classifyCollectionHelperSpelling(
    std::string_view path,
    CollectionCallShape shape,
    CollectionReceiverFamily receiverFamily,
    const CollectionDefinitionExistsFn &definitionExists) {
  CompatSpellingDecision decision;
  if (path.empty()) {
    return decision;
  }
  std::string rooted = path.front() == '/'
                           ? std::string(path)
                           : "/" + std::string(path);

  // Rule order (docs/CompatPathResolutionConsolidation.md, Goal):
  // shadow precedence first, rejection second, canonicalization last.

  // 1. Shadow precedence: a real definition at the spelled path always
  //    wins (rule-table rows 5, 13, 17; pinned by container_error_and_
  //    result_helpers.cpp:4059).
  if (definitionExists && definitionExists(rooted)) {
    return decision;
  }

  const std::string_view leaf = leafName(rooted);

  // 2. Rejection, checked before canonicalization so a retired spelling
  //    can never be rescued by the rename path.
  if (shape == CollectionCallShape::MethodCall) {
    if (receiverFamily == CollectionReceiverFamily::Soa &&
        classifierRetiredSoaReceiverMethodHelper(leaf)) {
      decision.disposition = CompatSpellingDisposition::Reject;
      decision.rejectReason = kRejectRetiredSoaMethod;
      return decision;
    }
    if ((startsWith(rooted, kArrayCompatPrefix) ||
         startsWith(rooted, kRootedVectorPrefix) ||
         startsWith(rooted, kCanonicalVectorPrefix)) &&
        classifierRemovedVectorCompatibilityHelper(leaf)) {
      // Rule-table rows 10, 12, 21.
      decision.disposition = CompatSpellingDisposition::Reject;
      decision.rejectReason = kRejectRemovedVectorMethod;
      return decision;
    }
    if (startsWith(rooted, kRootedMapPrefix) &&
        classifierRemovedKeyValueCompatibilityHelper(leaf)) {
      // Rule-table row 15.
      decision.disposition = CompatSpellingDisposition::Reject;
      decision.rejectReason = kRejectRemovedKeyValueMethod;
      return decision;
    }
  }

  // 3. Canonicalization, only when the canonical target definition exists
  //    (decision D5). Direct-call and binding-initializer shapes only:
  //    method-call canonicalization is receiver-driven and out of the
  //    classifier's scope.
  if (shape == CollectionCallShape::MethodCall) {
    return decision;
  }

  // Dead family (rule-table row 7): never canonicalize into or out of
  // /std/collections/soa_vector — it has no definitions.
  if (startsWith(rooted, kDeadSoaVectorPrefix) ||
      startsWith(rooted, kDeadCanonicalSoaVectorPrefix)) {
    return decision;
  }

  if (startsWith(rooted, kArrayCompatPrefix) &&
      !classifierRemovedVectorCompatibilityHelper(leaf)) {
    // Rule-table row 11.
    const std::string canonical = canonicalVectorHelperPath(leaf);
    if (definitionExists && definitionExists(canonical)) {
      decision.disposition = CompatSpellingDisposition::Canonicalize;
      decision.canonicalPath = canonical;
    }
    return decision;
  }

  if (startsWith(rooted, kBareSoaPrefix) &&
      classifierPublicSoaSurfaceHelperName(leaf)) {
    // Rule-table row 1, decision D1: bare public-surface SOA spellings
    // canonicalize when (and only when) the canonical definition exists.
    const std::string canonical = canonicalSoaHelperPath(leaf);
    if (definitionExists && definitionExists(canonical)) {
      decision.disposition = CompatSpellingDisposition::Canonicalize;
      decision.canonicalPath = canonical;
    }
    return decision;
  }

  // Registry-backed compat/lowering spellings (rule-table rows 14, 18, 20;
  // decision D6): canonicalize whenever the canonical target exists,
  // regardless of scope.
  if (const std::string canonical =
          resolveCompatibilitySpellingToCanonicalPath(rooted);
      !canonical.empty() && canonical != rooted) {
    if (definitionExists && definitionExists(canonical)) {
      decision.disposition = CompatSpellingDisposition::Canonicalize;
      decision.canonicalPath = canonical;
    }
    return decision;
  }

  return decision;
}

} // namespace primec
