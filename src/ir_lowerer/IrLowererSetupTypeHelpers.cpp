#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/support/CallNames.h"

#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

SetupTypeAdapters makeSetupTypeAdapters() {
  SetupTypeAdapters adapters;
  adapters.valueKindFromTypeName = makeValueKindFromTypeName();
  adapters.combineNumericKinds = makeCombineNumericKinds();
  return adapters;
}

ValueKindFromTypeNameFn makeValueKindFromTypeName() {
  return [](const std::string &name) {
    return valueKindFromTypeName(name);
  };
}

CombineNumericKindsFn makeCombineNumericKinds() {
  return [](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
    return combineNumericKinds(left, right);
  };
}

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name) {
  const std::string trimmed = trimTemplateTypeText(name);
  const size_t lastSlash = trimmed.rfind('/');
  const std::string normalized = (lastSlash == std::string::npos) ? trimmed : trimmed.substr(lastSlash + 1);

  switch (scalarTypeKindForName(normalized)) {
  case ScalarTypeKind::Int32:
    return LocalInfo::ValueKind::Int32;
  case ScalarTypeKind::Int64:
    return LocalInfo::ValueKind::Int64;
  case ScalarTypeKind::UInt64:
    return LocalInfo::ValueKind::UInt64;
  case ScalarTypeKind::UInt8:
    return LocalInfo::ValueKind::UInt8;
  case ScalarTypeKind::Float32:
    return LocalInfo::ValueKind::Float32;
  case ScalarTypeKind::Float64:
    return LocalInfo::ValueKind::Float64;
  case ScalarTypeKind::Bool:
    return LocalInfo::ValueKind::Bool;
  case ScalarTypeKind::String:
    return LocalInfo::ValueKind::String;
  case ScalarTypeKind::Void: // the lowerer has no kind for void
  case ScalarTypeKind::None:
    break;
  }
  if (normalized == "FileError" || normalized == "ImageError" || normalized == "ContainerError" ||
      normalized == "GfxError") {
    return LocalInfo::ValueKind::Int32;
  }

  std::string base;
  std::string arg;
  if (splitTemplateTypeName(normalized, base, arg)) {
    const std::string normalizedBase = normalizeCollectionBindingTypeName(base);
    if (normalizedBase == "File") {
      return LocalInfo::ValueKind::Int64;
    }
    if (normalizedBase == "Buffer") {
      return valueKindFromTypeName(trimTemplateTypeText(arg));
    }
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Unknown || right == LocalInfo::ValueKind::Unknown) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::String || right == LocalInfo::ValueKind::String) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Bool || right == LocalInfo::ValueKind::Bool) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Float32 || right == LocalInfo::ValueKind::Float32 ||
      left == LocalInfo::ValueKind::Float64 || right == LocalInfo::ValueKind::Float64) {
    if (left == LocalInfo::ValueKind::Float32 && right == LocalInfo::ValueKind::Float32) {
      return LocalInfo::ValueKind::Float32;
    }
    if (left == LocalInfo::ValueKind::Float64 && right == LocalInfo::ValueKind::Float64) {
      return LocalInfo::ValueKind::Float64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::UInt8 || right == LocalInfo::ValueKind::UInt8) {
    return (left == LocalInfo::ValueKind::UInt8 && right == LocalInfo::ValueKind::UInt8)
               ? LocalInfo::ValueKind::UInt8
               : LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::UInt64 || right == LocalInfo::ValueKind::UInt64) {
    return (left == LocalInfo::ValueKind::UInt64 && right == LocalInfo::ValueKind::UInt64)
               ? LocalInfo::ValueKind::UInt64
               : LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int64) {
    if ((left == LocalInfo::ValueKind::Int64 || left == LocalInfo::ValueKind::Int32) &&
        (right == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int32)) {
      return LocalInfo::ValueKind::Int64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int32 && right == LocalInfo::ValueKind::Int32) {
    return LocalInfo::ValueKind::Int32;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind comparisonKind(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Bool) {
    left = LocalInfo::ValueKind::Int32;
  }
  if (right == LocalInfo::ValueKind::Bool) {
    right = LocalInfo::ValueKind::Int32;
  }
  return combineNumericKinds(left, right);
}

std::string typeNameForValueKind(LocalInfo::ValueKind kind) {
  switch (kind) {
    case LocalInfo::ValueKind::Int32:
      return "i32";
    case LocalInfo::ValueKind::Int64:
      return "i64";
    case LocalInfo::ValueKind::UInt64:
      return "u64";
      case LocalInfo::ValueKind::UInt8:
      return "u8";
    case LocalInfo::ValueKind::Float32:
      return "f32";
    case LocalInfo::ValueKind::Float64:
      return "f64";
    case LocalInfo::ValueKind::Bool:
      return "bool";
    case LocalInfo::ValueKind::String:
      return "string";
    default:
      return "";
  }
}

} // namespace primec::ir_lowerer
