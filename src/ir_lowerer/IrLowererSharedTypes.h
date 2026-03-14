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
  bool isFileHandle = false;
  bool isFileError = false;
  bool isResult = false;
  bool resultHasValue = false;
  ValueKind resultValueKind = ValueKind::Unknown;
  std::string resultErrorType;
  enum class StringSource { None, TableIndex, ArgvIndex, RuntimeIndex } stringSource = StringSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  bool referenceToArray = false;
  bool referenceToMap = false;
  bool isUninitializedStorage = false;
  bool isSoaVector = false;
};

using LocalMap = std::unordered_map<std::string, LocalInfo>;

struct ReturnInfo {
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind kind = LocalInfo::ValueKind::Unknown;
  bool isResult = false;
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
};

} // namespace primec::ir_lowerer
