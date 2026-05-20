


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
