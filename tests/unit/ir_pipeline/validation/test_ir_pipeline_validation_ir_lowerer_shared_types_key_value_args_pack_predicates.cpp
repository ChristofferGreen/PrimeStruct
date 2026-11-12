#include "test_ir_pipeline_validation_helpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/support/CollectionSpellingClassifier.h"

// TODO-5291 (see docs/todo_finished.md): these two test cases are this
// codebase's direct, millisecond-fast unit-level regression net for the
// exact seam TODO-4760(a) broke and fixed (commit 181c22a) - the
// map-vs-Entry args-pack-element receiver discriminator and the
// elemSlotCount-based load-vs-copy decision. Before that fix, neither
// named predicate below existed at all: `isKeyValueAccessTarget` in
// IrLowererLowerStatementsExpr.h deferred ANY key-value-shaped args-pack
// receiver (map or Entry alike) to key-value access with no discriminator,
// and `isInlineMapArgsPackTarget` in IrLowererIndexedAccessEmit.cpp used
// `elemSlotCount > 0` (not `> 1`), so a single-heap-pointer-backed map
// element (elemSlotCount == 1) was wrongly treated as a multi-slot struct
// needing an address-only copy - the exact bug that made `count()` return
// a garbage value (100) instead of the correct count. Verified this pins
// real, fixed behavior (not just restating current code): checked out
// commit e4cd1c8 (immediately before the 181c22a fix) in a scratch
// worktree and confirmed (a) `isMapArgsPackElement`/
// `isSingleSlotPointerStyleKeyValueStorage` do not exist anywhere in that
// commit's IrLowererSharedTypes.h (grep: zero matches) - this test file
// could not even compile against it: and (b) a standalone reproduction of
// that commit's literal `elemSlotCount > 0` formula (copied verbatim from
// IrLowererIndexedAccessEmit.cpp:306-310 at e4cd1c8) misclassifies
// elemSlotCount == 1 as needing a struct copy, while the current
// `isSingleSlotPointerStyleKeyValueStorage`-based `> 1` decision (tested
// below) correctly does not.

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("isMapArgsPackElement flags a bare args<map<K,V>> pack element and excludes Entry<K,V>'s own pack") {
  using primec::ir_lowerer::LocalInfo;
  using primec::ir_lowerer::isMapArgsPackElement;

  // A bare `args<map<K, V>>` pack element: key/value kinds resolved, but no
  // backing struct path (nothing populates structTypeName for this shape).
  LocalInfo mapArgsPackElement;
  mapArgsPackElement.isArgsPack = true;
  mapArgsPackElement.keyValueKeyKind = LocalInfo::ValueKind::Int64;
  mapArgsPackElement.keyValueValueKind = LocalInfo::ValueKind::Int64;
  CHECK(mapArgsPackElement.structTypeName.empty());
  CHECK(isMapArgsPackElement(mapArgsPackElement));

  // The stdlib map constructor's own internal `args<Entry<K, V>>` pack
  // element: also key-value-shaped, but its structTypeName is populated
  // with a concrete Entry__t...-rooted path.
  LocalInfo entryArgsPackElement = mapArgsPackElement;
  entryArgsPackElement.structTypeName = "/std/collections/map/Entry__t_i64_i64";
  CHECK_FALSE(isMapArgsPackElement(entryArgsPackElement));

  // A non-key-value LocalInfo (no key/value kinds resolved) is never a map
  // args-pack element, regardless of structTypeName.
  LocalInfo nonKeyValueInfo;
  nonKeyValueInfo.isArgsPack = true;
  CHECK_FALSE(isMapArgsPackElement(nonKeyValueInfo));

  // Only one of the two kinds resolved is not enough (mirrors hasKeyValueKinds).
  LocalInfo partialKeyValueInfo;
  partialKeyValueInfo.isArgsPack = true;
  partialKeyValueInfo.keyValueKeyKind = LocalInfo::ValueKind::Int64;
  CHECK_FALSE(isMapArgsPackElement(partialKeyValueInfo));
}

TEST_CASE("isSingleSlotPointerStyleKeyValueStorage distinguishes pointer-style storage from inline struct copies") {
  using primec::ir_lowerer::isSingleSlotPointerStyleKeyValueStorage;

  // Exactly one slot: stored as a single heap pointer, same convention used
  // for builtin-storage map<K,V>(...) bindings.
  CHECK(isSingleSlotPointerStyleKeyValueStorage(1));

  // More than one slot: an inline multi-slot struct needing an
  // address-only copy, not a pointer load.
  CHECK_FALSE(isSingleSlotPointerStyleKeyValueStorage(2));
  CHECK_FALSE(isSingleSlotPointerStyleKeyValueStorage(3));

  // Zero (or negative, defensively) slots is neither convention.
  CHECK_FALSE(isSingleSlotPointerStyleKeyValueStorage(0));
  CHECK_FALSE(isSingleSlotPointerStyleKeyValueStorage(-1));
}

TEST_CASE("lowerer collection spelling helpers come from the classifier") {
  // Only the leaf is stripped, so a `__` in a namespace segment survives.
  CHECK(primec::classifierStripGeneratedHelperSuffix("count__t1a2b") == "count");
  CHECK(primec::classifierStripGeneratedHelperSuffix("/std/collections/vector/push__ov3") ==
        "/std/collections/vector/push");
  CHECK(primec::classifierStripGeneratedHelperSuffix("/ns__x/count") == "/ns__x/count");
  CHECK(primec::classifierStripGeneratedHelperSuffix("count") == "count");

  CHECK_FALSE(primec::classifierArrayVectorCompatibilitySuffixAllowed("count"));
  CHECK_FALSE(primec::classifierArrayVectorCompatibilitySuffixAllowed("remove_swap"));
  CHECK(primec::classifierArrayVectorCompatibilitySuffixAllowed("insert"));

  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.name = "count";
  CHECK(primec::ir_lowerer::resolveScopedCallPath(call) == "count");
  call.namespacePrefix = "/";
  CHECK(primec::ir_lowerer::resolveScopedCallPath(call) == "/count");
  call.namespacePrefix = "/ns";
  CHECK(primec::ir_lowerer::resolveScopedCallPath(call) == "/ns/count");
  call.namespacePrefix = "/ns/";
  CHECK(primec::ir_lowerer::resolveScopedCallPath(call) == "/ns/count");
  call.name = "a/count";
  CHECK(primec::ir_lowerer::resolveScopedCallPath(call) == "a/count");
}

TEST_CASE("lowerer vector record paths and collection helper candidates have one definition") {
  using namespace primec::ir_lowerer;
  CHECK(experimentalCollectionTypePath("vector", "Vector") == "/std/collections/vector/Vector");
  CHECK(experimentalCollectionTypePath("map", "Map") == "/std/collections/experimental_map/Map");

  // Record paths are resolved paths: the backing path and its `__` specializations only.
  const std::string vectorPath = vectorBackingTypePath();
  CHECK(isCollectionVectorRecordPath(vectorPath));
  CHECK(isCollectionVectorRecordPath(vectorPath + "__t1a2b"));
  CHECK_FALSE(isCollectionVectorRecordPath("Vector"));
  CHECK_FALSE(isCollectionVectorRecordPath("Vector<i32>"));
  CHECK_FALSE(isCollectionVectorRecordPath(vectorPath + "Extra"));
  // Type text may also be bare, so a caller reading unresolved text uses the wider check.
  CHECK(isCollectionVectorTypeText(vectorPath));
  CHECK(isCollectionVectorTypeText("Vector<i32>"));
  CHECK_FALSE(isCollectionVectorTypeText("Other<i32>"));

  // Candidates keep the path as written, then its rooted form with the generated suffix dropped.
  const auto plain = collectionHelperPathCandidates("array/count");
  REQUIRE(plain.size() == 2);
  CHECK(plain[0] == "array/count");
  CHECK(plain[1] == "/array/count");
  const auto suffixed = collectionHelperPathCandidates("vector/push__t1a2b");
  REQUIRE(suffixed.size() == 2);
  CHECK(suffixed[0] == "vector/push__t1a2b");
  CHECK(suffixed[1] == "/vector/push");
  // `/array/<name>` adds the canonical vector helper unless the name is a removed alias or the
  // registry has no such helper.
  CHECK(collectionHelperPathCandidates("/array/count").size() == 1);
  const std::string canonicalInsert = primec::stdlibSurfaceCanonicalHelperPath(
      primec::StdlibSurfaceId::CollectionsManifestSurface0, "insert");
  CHECK(collectionHelperPathCandidates("/array/insert").size() ==
        (canonicalInsert.empty() ? 1u : 2u));
}

TEST_SUITE_END();
