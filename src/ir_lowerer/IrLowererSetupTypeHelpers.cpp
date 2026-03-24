#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"

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
  if (name == "int" || name == "i32") {
    return LocalInfo::ValueKind::Int32;
  }
  if (name == "i64") {
    return LocalInfo::ValueKind::Int64;
  }
  if (name == "u64") {
    return LocalInfo::ValueKind::UInt64;
  }
  if (name == "float" || name == "f32") {
    return LocalInfo::ValueKind::Float32;
  }
  if (name == "f64") {
    return LocalInfo::ValueKind::Float64;
  }
  if (name == "bool") {
    return LocalInfo::ValueKind::Bool;
  }
  if (name == "string") {
    return LocalInfo::ValueKind::String;
  }
  if (name == "FileError" || name == "ImageError" || name == "ContainerError" ||
      name == "GfxError") {
    return LocalInfo::ValueKind::Int32;
  }

  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg) && base == "File") {
    return LocalInfo::ValueKind::Int64;
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

bool inferReceiverTypeFromDeclaredReturn(const Definition &definition, std::string &typeNameOut) {
  std::vector<std::string> collectionArgs;
  if (inferDeclaredReturnCollection(definition, typeNameOut, collectionArgs)) {
    return true;
  }

  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &declaredType = transform.templateArgs.front();
    if (declaredType.empty() || declaredType == "void" || declaredType == "auto") {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(declaredType, base, argText)) {
      // Non-collection templated returns are not method receiver targets.
      return false;
    }
    typeNameOut = declaredType;
    if (!typeNameOut.empty() && typeNameOut.front() == '/') {
      typeNameOut.erase(0, 1);
    }
    return !typeNameOut.empty();
  }
  return false;
}

} // namespace primec::ir_lowerer
