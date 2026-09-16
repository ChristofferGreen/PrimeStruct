#include "test_ir_pipeline_validation_helpers.h"

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

TEST_SUITE_END();
