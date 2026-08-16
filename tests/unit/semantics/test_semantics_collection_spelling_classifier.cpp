#include "third_party/doctest.h"

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

TEST_SUITE_BEGIN("primestruct.semantics.collection_spelling_classifier");

namespace {

using primec::classifyCollectionHelperSpelling;
using primec::CollectionCallShape;
using primec::CollectionReceiverFamily;
using primec::CompatSpellingDecision;
using primec::CompatSpellingDisposition;

primec::CollectionDefinitionExistsFn definitionSet(std::set<std::string> paths) {
  return [paths = std::move(paths)](std::string_view path) {
    return paths.count(std::string(path)) > 0;
  };
}

const std::vector<std::string> kRemovedVectorHelpers = {
    "count", "capacity", "at", "at_unsafe", "push",
    "pop",   "reserve",  "clear", "remove_at", "remove_swap"};

const std::vector<std::string> kRemovedBorrowedSoaHelpers = {
    "count_ref", "get_ref", "ref_ref", "to_aos_ref"};

const std::vector<std::string> kRemovedKeyValueHelpers = {
    "count", "count_ref", "size", "contains", "contains_ref",
    "tryAt", "tryAt_ref", "at", "at_ref", "at_unsafe",
    "at_unsafe_ref", "insert", "insert_ref"};

} // namespace

// Decision D2: the classifier's sets are the single source; assert they
// match the historical hardcoded lists exactly (10 / 4 / 13 names), so any
// future registry-driven replacement must preserve these memberships.
TEST_CASE("classifier removed-helper sets match the historical name lists") {
  for (const auto &name : kRemovedVectorHelpers) {
    CHECK(primec::classifierRemovedVectorCompatibilityHelper(name));
  }
  CHECK_FALSE(primec::classifierRemovedVectorCompatibilityHelper("get"));
  CHECK_FALSE(primec::classifierRemovedVectorCompatibilityHelper("ref"));

  for (const auto &name : kRemovedBorrowedSoaHelpers) {
    CHECK(primec::classifierRemovedBorrowedSoaCompatibilityHelper(name));
  }
  CHECK_FALSE(primec::classifierRemovedBorrowedSoaCompatibilityHelper("ref"));
  CHECK_FALSE(primec::classifierRemovedBorrowedSoaCompatibilityHelper("get"));

  for (const auto &name : kRemovedKeyValueHelpers) {
    CHECK(primec::classifierRemovedKeyValueCompatibilityHelper(name));
  }
  CHECK_FALSE(primec::classifierRemovedKeyValueCompatibilityHelper("keys"));
}

// Decision D2, registry-agreement half: the registry-membership variants
// used by SemanticsValidatorExprMethodTargetResolution.cpp treat the
// vector manifest surface (CollectionsManifestSurface0) and the key-value
// manifest surface (CollectionsManifestSurface2, plus "size") as the
// removed sets. Pin that those registry memberships cover the historical
// lists so the two implementation families cannot drift silently.
TEST_CASE("registry manifest surfaces agree with the removed-helper name lists") {
  for (const auto &name : kRemovedVectorHelpers) {
    CHECK_MESSAGE(
        primec::isStdlibSurfaceMemberName(
            primec::StdlibSurfaceId::CollectionsManifestSurface0, name),
        "vector manifest surface is missing removed helper: " << name);
  }
  for (const auto &name : kRemovedKeyValueHelpers) {
    if (name == "size") {
      // "size" is bolted onto the registry variant separately
      // (SemanticsValidatorExprMethodTargetResolution.cpp:22-23) and is
      // asserted in the hardcoded-list test above instead.
      continue;
    }
    CHECK_MESSAGE(
        primec::isStdlibSurfaceMemberName(
            primec::StdlibSurfaceId::CollectionsManifestSurface2, name),
        "key-value manifest surface is missing removed helper: " << name);
  }
}

// Rule-table row 5 / decision D3: in method shape a definition at the
// spelled path wins before rejection (Mechanism B's visible-same-path
// escape, pinned upstream by container_error_and_result_helpers.cpp:4059).
// In non-method shapes bare public-SOA spellings canonicalize
// shadow-blind: the binding-inference consumer's reference behavior
// surfaces a divergent shadow signature through ordinary type checking
// instead of silently preferring it (pinned upstream by :4093).
TEST_CASE("shadow definition wins in method shape but not for bare soa direct shape") {
  const auto defs = definitionSet({"/soa/ref", "/std/collections/soa/ref"});

  const CompatSpellingDecision method = classifyCollectionHelperSpelling(
      "/soa/ref", CollectionCallShape::MethodCall,
      CollectionReceiverFamily::Soa, defs);
  CHECK(method.disposition == CompatSpellingDisposition::PassThrough);

  const CompatSpellingDecision direct = classifyCollectionHelperSpelling(
      "/soa/ref", CollectionCallShape::DirectCall,
      CollectionReceiverFamily::None, defs);
  CHECK(direct.disposition == CompatSpellingDisposition::Canonicalize);
  CHECK(direct.canonicalPath == "/std/collections/soa/ref");

  // Non-SOA families keep shadow-first in every shape.
  const auto arrayDefs = definitionSet({"/array/get"});
  const CompatSpellingDecision arrayShadow = classifyCollectionHelperSpelling(
      "/array/get", CollectionCallShape::DirectCall,
      CollectionReceiverFamily::None, arrayDefs);
  CHECK(arrayShadow.disposition == CompatSpellingDisposition::PassThrough);
}

// Rule-table rows 2-3 (Mechanism B): retired SOA receiver methods reject,
// even though `ref`, `push`, `reserve` are absent from the isRemoved*
// sets. Pinned upstream by container_error_and_result_helpers.cpp:1691
// and :1807.
TEST_CASE("retired soa receiver methods reject without a shadow") {
  const auto defs = definitionSet({"/std/collections/soa/get",
                                   "/std/collections/soa/ref"});
  for (const std::string helper : {"ref", "push", "reserve", "get_ref",
                                   "ref_ref", "count_ref", "to_aos_ref"}) {
    const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
        "/soa/" + helper, CollectionCallShape::MethodCall,
        CollectionReceiverFamily::Soa, defs);
    CHECK_MESSAGE(
        decision.disposition == CompatSpellingDisposition::Reject,
        "expected reject for retired soa method: " << helper);
  }
}

// Rule-table row 4: supported SOA access methods are not rejected by the
// classifier (they pass through to receiver-driven resolution).
TEST_CASE("supported soa access methods pass through in method shape") {
  const auto defs = definitionSet({"/std/collections/soa/get"});
  const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
      "/soa/get", CollectionCallShape::MethodCall,
      CollectionReceiverFamily::Soa, defs);
  CHECK(decision.disposition == CompatSpellingDisposition::PassThrough);
}

// Rule-table row 1 / decision D1 (as refined by the Step 1 differential
// audit): bare public-surface SOA direct calls canonicalize to the stdlib
// target unconditionally - the target is a real stdlib surface, and
// wrapper-return routing tests depend on the rename even when the
// canonical family is not in the current stage's definition map. Only a
// same-path shadow definition suppresses it (covered by the shadow test
// above).
TEST_CASE("bare soa direct calls canonicalize unconditionally") {
  for (const auto &defs :
       {definitionSet({"/std/collections/soa/get"}), definitionSet({})}) {
    const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
        "/soa/get", CollectionCallShape::DirectCall,
        CollectionReceiverFamily::None, defs);
    CHECK(decision.disposition == CompatSpellingDisposition::Canonicalize);
    CHECK(decision.canonicalPath == "/std/collections/soa/get");
  }
}

// Rule-table row 7 / decision D5: the soa_vector family is dead; the
// classifier never canonicalizes into or out of it.
TEST_CASE("soa_vector spellings always pass through") {
  const auto defs = definitionSet({"/std/collections/soa/get"});
  for (const std::string path :
       {"/soa_vector/get", "/std/collections/soa_vector/get"}) {
    const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
        path, CollectionCallShape::DirectCall,
        CollectionReceiverFamily::None, defs);
    CHECK_MESSAGE(
        decision.disposition == CompatSpellingDisposition::PassThrough,
        "expected pass-through for dead-family path: " << path);
  }
}

// Rule-table rows 10, 12, 21: removed vector helpers reject in method
// shape across the /array, /vector, and canonical-vector namespaces.
TEST_CASE("removed vector helpers reject in method shape") {
  const auto defs = definitionSet({});
  for (const std::string prefix :
       {"/array/", "/vector/", "/std/collections/vector/"}) {
    const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
        prefix + "push", CollectionCallShape::MethodCall,
        CollectionReceiverFamily::Vector, defs);
    CHECK_MESSAGE(
        decision.disposition == CompatSpellingDisposition::Reject,
        "expected reject for removed vector method: " << prefix << "push");
  }
}

// Rule-table rows 9, 13 / decision D3: the same removed spellings pass
// through in direct-call shape so shadows stay reachable and rejection
// stays emergent.
TEST_CASE("removed vector helpers pass through in direct shape") {
  const auto defs = definitionSet({});
  const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
      "/array/push", CollectionCallShape::DirectCall,
      CollectionReceiverFamily::None, defs);
  CHECK(decision.disposition == CompatSpellingDisposition::PassThrough);
}

// Rule-table row 11: non-removed /array helpers canonicalize to the
// vector family when the canonical definition exists.
TEST_CASE("array compat helpers canonicalize to vector when def exists") {
  const auto defs = definitionSet({"/std/collections/vector/get"});
  const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
      "/array/get", CollectionCallShape::DirectCall,
      CollectionReceiverFamily::None, defs);
  CHECK(decision.disposition == CompatSpellingDisposition::Canonicalize);
  CHECK(decision.canonicalPath == "/std/collections/vector/get");
}

// Identity canonicalization: already-canonical /std/collections member
// spellings with an existing definition are the positive answer in
// non-method shapes (pinned upstream by "vector stdlib namespaced count
// helper auto inference falls back to canonical helper return"), while
// removed canonical-namespace method spellings still reject (row 21).
TEST_CASE("canonical spellings with existing defs canonicalize to themselves") {
  const auto defs = definitionSet({"/std/collections/vector/count"});
  const CompatSpellingDecision direct = classifyCollectionHelperSpelling(
      "/std/collections/vector/count", CollectionCallShape::DirectCall,
      CollectionReceiverFamily::None, defs);
  CHECK(direct.disposition == CompatSpellingDisposition::Canonicalize);
  CHECK(direct.canonicalPath == "/std/collections/vector/count");

  // With the definition present, method shape resolves through it
  // (shadow pass-through); the row-21 reject applies when no definition
  // exists.
  const CompatSpellingDecision methodWithDef = classifyCollectionHelperSpelling(
      "/std/collections/vector/count", CollectionCallShape::MethodCall,
      CollectionReceiverFamily::Vector, defs);
  CHECK(methodWithDef.disposition == CompatSpellingDisposition::PassThrough);

  const CompatSpellingDecision methodNoDef = classifyCollectionHelperSpelling(
      "/std/collections/vector/count", CollectionCallShape::MethodCall,
      CollectionReceiverFamily::Vector, definitionSet({}));
  CHECK(methodNoDef.disposition == CompatSpellingDisposition::Reject);
}

// Rule-table row 15: removed key-value helpers reject in method shape
// under the map root.
TEST_CASE("removed key-value helpers reject in method shape") {
  const auto defs = definitionSet({});
  const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
      "/map/insert", CollectionCallShape::MethodCall,
      CollectionReceiverFamily::Map, defs);
  CHECK(decision.disposition == CompatSpellingDisposition::Reject);
}

// Rule-table row 17: the same key-value spellings pass through in direct
// shape (decline-before-canonicalize; shadow reachability).
TEST_CASE("removed key-value helpers pass through in direct shape") {
  const auto defs = definitionSet({});
  const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
      "/map/insert", CollectionCallShape::DirectCall,
      CollectionReceiverFamily::None, defs);
  CHECK(decision.disposition == CompatSpellingDisposition::PassThrough);
}

TEST_SUITE_END();
