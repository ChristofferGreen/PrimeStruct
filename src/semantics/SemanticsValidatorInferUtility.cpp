#include "SemanticsValidator.h"

namespace primec::semantics {

ReturnKind SemanticsValidator::combineInferredNumericKinds(ReturnKind left, ReturnKind right) const {
  if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::String ||
      right == ReturnKind::String || left == ReturnKind::Void || right == ReturnKind::Void ||
      left == ReturnKind::Array || right == ReturnKind::Array) {
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
    return ReturnKind::Float64;
  }
  if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
    return ReturnKind::Float32;
  }
  if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
    return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
  }
  if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
    if ((left == ReturnKind::Int64 || left == ReturnKind::Int) &&
        (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
      return ReturnKind::Int64;
    }
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Int && right == ReturnKind::Int) {
    return ReturnKind::Int;
  }
  return ReturnKind::Unknown;
}

bool SemanticsValidator::isInferStructTypeName(const std::string &typeName,
                                               const std::string &namespacePrefix) const {
  if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
    return false;
  }
  std::string resolved = resolveTypePath(typeName, namespacePrefix);
  if (structNames_.count(resolved) > 0) {
    return true;
  }
  auto importIt = importAliases_.find(typeName);
  if (importIt != importAliases_.end()) {
    return structNames_.count(importIt->second) > 0;
  }
  return false;
}

ReturnKind SemanticsValidator::inferReferenceTargetKind(const std::string &templateArg,
                                                        const std::string &namespacePrefix) const {
  if (templateArg.empty()) {
    return ReturnKind::Unknown;
  }
  ReturnKind kind = returnKindForTypeName(templateArg);
  if (kind != ReturnKind::Unknown) {
    return kind;
  }
  if (isInferStructTypeName(templateArg, namespacePrefix)) {
    return ReturnKind::Array;
  }
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(templateArg, base, arg) && normalizeBindingTypeName(base) == "array") {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
      return ReturnKind::Array;
    }
  }
  if (splitTemplateTypeName(templateArg, base, arg) && isMapCollectionTypeName(base)) {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
      return ReturnKind::Array;
    }
  }
  return ReturnKind::Unknown;
}

ReturnKind SemanticsValidator::inferUninitializedTargetKind(const std::string &templateArg,
                                                            const std::string &namespacePrefix) const {
  if (templateArg.empty()) {
    return ReturnKind::Unknown;
  }
  ReturnKind kind = returnKindForTypeName(templateArg);
  if (kind != ReturnKind::Unknown) {
    return kind;
  }
  if (isInferStructTypeName(templateArg, namespacePrefix)) {
    return ReturnKind::Array;
  }
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(templateArg, base, arg) && normalizeBindingTypeName(base) == "array") {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
      return ReturnKind::Array;
    }
  }
  if (splitTemplateTypeName(templateArg, base, arg) && isMapCollectionTypeName(base)) {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
      return ReturnKind::Array;
    }
  }
  return ReturnKind::Unknown;
}

} // namespace primec::semantics
