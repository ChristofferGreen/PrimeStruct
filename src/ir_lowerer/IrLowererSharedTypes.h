#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace primec::ir_lowerer {

struct LocalInfo {
  int32_t index = 0;
  bool isMutable = false;
  enum class Kind { Value, Pointer, Reference, Array, Vector, Map, Buffer } kind = Kind::Value;
  enum class ValueKind { Unknown, Int32, Int64, UInt64, Float32, Float64, Bool, String } valueKind = ValueKind::Unknown;
  std::string structTypeName;
  int32_t structFieldCount = 0;
  ValueKind mapKeyKind = ValueKind::Unknown;
  ValueKind mapValueKind = ValueKind::Unknown;
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
  bool referenceToMap = false;
  bool pointerToMap = false;
  bool isUninitializedStorage = false;
  bool targetsUninitializedStorage = false;
  bool isSoaVector = false;
  bool usesBuiltinCollectionLayout = false;
};

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

} // namespace primec::ir_lowerer
