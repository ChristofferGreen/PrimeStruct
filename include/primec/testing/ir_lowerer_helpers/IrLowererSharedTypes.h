


struct LocalInfo {
  int32_t index = 0;
  bool isMutable = false;
  enum class Kind { Value, Pointer, Reference, Array, Vector, Buffer } kind = Kind::Value;
  enum class ValueKind { Unknown, Int32, Int64, UInt64, Float32, Float64, Bool, String } valueKind = ValueKind::Unknown;
  std::string structTypeName;
  int32_t structFieldCount = 0;
  ValueKind keyValueKeyKind = ValueKind::Unknown;
  ValueKind keyValueValueKind = ValueKind::Unknown;
  Kind argsPackElementKind = Kind::Value;
  int32_t structSlotCount = 0;
  bool isFileHandle = false;
  bool isFileError = false;
  std::string errorTypeName;
  std::string errorHelperNamespacePath;
  bool isResult = false;
  bool resultHasValue = false;
  ValueKind resultValueKind = ValueKind::Unknown;
  Kind resultValueCollectionKind = Kind::Value;
  ValueKind resultValueMapKeyKind = ValueKind::Unknown;
  bool resultValueIsFileHandle = false;
  std::string resultValueStructType;
  std::string resultErrorType;
  enum class StringSource { None, TableIndex, ArgvIndex, RuntimeIndex } stringSource = StringSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  bool isArgsPack = false;
  int32_t argsPackElementCount = -1;
  bool referenceToArray = false;
  bool pointerToArray = false;
  bool referenceToVector = false;
  bool pointerToVector = false;
  bool referenceToBuffer = false;
  bool pointerToBuffer = false;
  bool isUninitializedStorage = false;
  bool targetsUninitializedStorage = false;
  bool isSoaVector = false;
  bool usesBuiltinCollectionLayout = false;
};

inline bool hasKeyValueKinds(const LocalInfo &info) {
  return info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
         info.keyValueValueKind != LocalInfo::ValueKind::Unknown;
}

inline bool hasWrappedKeyValueKinds(const LocalInfo &info, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         hasKeyValueKinds(info);
}

// A `LocalInfo` for a bare `args<map<K, V>>` pack element (a user-level
// variadic parameter whose elements are key-value pairs) has key/value kinds
// set (per `hasKeyValueKinds`) but an EMPTY `structTypeName` - it never gets
// a backing struct path resolved for it. Contrast this with a `LocalInfo`
// for `args<Entry<K, V>>` (the stdlib map constructor's own internal args
// pack, used to build the map's storage) - it is *also* key-value-shaped per
// `hasKeyValueKinds`, but its `structTypeName` is always populated with a
// concrete `Entry__t...`-rooted path. `hasKeyValueKinds` alone cannot tell
// these two args-pack-element shapes apart; this predicate names the
// distinguishing signal (empty vs. populated `structTypeName`) so call sites
// that need to single out the bare `args<map<K,V>>` case don't have to
// re-derive it ad hoc. See docs/ReceiverTargetResolutionConsolidation.md and
// docs/todo_finished.md (TODO-4760(a)) for how this was discovered.
inline bool isMapArgsPackElement(const LocalInfo &info) {
  return hasKeyValueKinds(info) && info.structTypeName.empty();
}

// A key-value args-pack element with exactly one storage slot is
// materialized as a single heap pointer - the same "builtin key/value map
// materialized as a heap pointer" convention `tryEmitBuiltinKeyValueConstructor`
// uses for bare `map<K, V>(...)` bindings elsewhere (see
// `IrLowererLowerStatementsBindings.h`'s `hasKeyValueKinds` branch, which
// stores that single pointer directly instead of struct-copying a backing
// layout). A key-value args-pack element with MORE than one slot is instead
// an inline multi-slot struct that needs an address-only copy, not a pointer
// load. This predicate takes the raw slot count (rather than a `LocalInfo`)
// because the slot count for an args-pack element is a target-resolution
// fact computed onto `ArrayVectorAccessTargetInfo`/similar helper structs
// (see `IrLowererCallHelperTypes.h`), not a field stored on `LocalInfo`
// itself.
inline bool isSingleSlotPointerStyleKeyValueStorage(int32_t elemSlotCount) {
  return elemSlotCount == 1;
}

using LocalMap = std::unordered_map<std::string, LocalInfo>;

struct ReturnInfo {
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind kind = LocalInfo::ValueKind::Unknown;
  bool isResult = false;
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::Kind resultValueCollectionKind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
  bool resultValueIsFileHandle = false;
  std::string resultValueStructType;
  std::string resultErrorType;
};
