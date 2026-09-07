#include "test_ir_pipeline_validation_helpers.h"

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
