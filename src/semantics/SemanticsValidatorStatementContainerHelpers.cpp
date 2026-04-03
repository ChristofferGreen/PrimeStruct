#include "SemanticsValidator.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool templateArgsContainTypeName(const std::vector<std::string> *templateArgs, const std::string &typeName) {
  if (templateArgs == nullptr) {
    return false;
  }
  const std::string normalized = normalizeBindingTypeName(typeName);
  for (const auto &candidate : *templateArgs) {
    if (normalizeBindingTypeName(candidate) == normalized) {
      return true;
    }
  }
  return false;
}

} // namespace

bool SemanticsValidator::isDropTrivialContainerElementType(const std::string &typeName,
                                                           const std::string &namespacePrefix,
                                                           const std::vector<std::string> *definitionTemplateArgs,
                                                           std::unordered_set<std::string> &visitingStructs) {
  if (templateArgsContainTypeName(definitionTemplateArgs, typeName)) {
    return true;
  }

  const std::string normalizedType = normalizeBindingTypeName(typeName);
  if (normalizedType == "bool" || normalizedType == "i32" || normalizedType == "i64" || normalizedType == "u64" ||
      normalizedType == "f32" || normalizedType == "f64" || normalizedType == "string") {
    return true;
  }

  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (templateArgsContainTypeName(definitionTemplateArgs, normalizedBase)) {
      return true;
    }
    if (normalizedBase == "Pointer" || normalizedBase == "Reference") {
      return true;
    }
    if (normalizedBase == "array") {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(argText, args) && args.size() == 1 &&
             isDropTrivialContainerElementType(args.front(), namespacePrefix, definitionTemplateArgs, visitingStructs);
    }
    if (normalizedBase == "vector" || normalizedBase == "map" || normalizedBase == "soa_vector" ||
        normalizedBase == "uninitialized" || normalizedBase == "Buffer") {
      return false;
    }
    base = normalizedBase;
  } else {
    base = normalizedType;
  }

  const std::string structPath = resolveStructTypePath(base, namespacePrefix, structNames_);
  if (structPath.rfind("/std/collections/experimental_map", 0) == 0) {
    return true;
  }
  if (structPath.empty() || structNames_.count(structPath) == 0) {
    return true;
  }
  if (!visitingStructs.insert(structPath).second) {
    return true;
  }

  struct VisitingScope {
    std::unordered_set<std::string> &set;
    std::string value;
    ~VisitingScope() { set.erase(value); }
  } visitingScope{visitingStructs, structPath};

  if (defMap_.count(structPath + "/Destroy") > 0 || defMap_.count(structPath + "/DestroyStack") > 0 ||
      defMap_.count(structPath + "/DestroyHeap") > 0 || defMap_.count(structPath + "/DestroyBuffer") > 0) {
    return false;
  }

  const Definition *structDef = nullptr;
  auto defIt = defMap_.find(structPath);
  if (defIt != defMap_.end()) {
    structDef = defIt->second;
  }
  if (structDef == nullptr) {
    return true;
  }

  for (const auto &fieldStmt : structDef->statements) {
    if (!fieldStmt.isBinding) {
      continue;
    }
    BindingInfo fieldBinding;
    if (!resolveStructFieldBinding(*structDef, fieldStmt, fieldBinding)) {
      continue;
    }
    if (!isDropTrivialContainerElementType(bindingTypeText(fieldBinding),
                                           structDef->namespacePrefix,
                                           &structDef->templateArgs,
                                           visitingStructs)) {
      return false;
    }
  }

  return true;
}

bool SemanticsValidator::isRelocationTrivialContainerElementType(const std::string &typeName,
                                                                 const std::string &namespacePrefix,
                                                                 const std::vector<std::string> *definitionTemplateArgs,
                                                                 std::unordered_set<std::string> &visitingStructs) {
  if (templateArgsContainTypeName(definitionTemplateArgs, typeName)) {
    return true;
  }

  const std::string normalizedType = normalizeBindingTypeName(typeName);
  if (normalizedType == "bool" || normalizedType == "i32" || normalizedType == "i64" || normalizedType == "u64" ||
      normalizedType == "f32" || normalizedType == "f64" || normalizedType == "string") {
    return true;
  }

  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (templateArgsContainTypeName(definitionTemplateArgs, normalizedBase)) {
      return true;
    }
    if (normalizedBase == "Pointer" || normalizedBase == "Reference") {
      return true;
    }
    if (normalizedBase == "array") {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(argText, args) && args.size() == 1 &&
             isRelocationTrivialContainerElementType(args.front(),
                                                     namespacePrefix,
                                                     definitionTemplateArgs,
                                                     visitingStructs);
    }
    if (normalizedBase == "vector" || normalizedBase == "map" || normalizedBase == "soa_vector" ||
        normalizedBase == "uninitialized" || normalizedBase == "Buffer") {
      return false;
    }
    base = normalizedBase;
  } else {
    base = normalizedType;
  }

  const std::string structPath = resolveStructTypePath(base, namespacePrefix, structNames_);
  if (structPath.rfind("/std/collections/experimental_map", 0) == 0) {
    return true;
  }
  if (structPath.empty() || structNames_.count(structPath) == 0) {
    return true;
  }
  if (!visitingStructs.insert(structPath).second) {
    return true;
  }

  struct VisitingScope {
    std::unordered_set<std::string> &set;
    std::string value;
    ~VisitingScope() { set.erase(value); }
  } visitingScope{visitingStructs, structPath};

  if (defMap_.count(structPath + "/Destroy") > 0 || defMap_.count(structPath + "/DestroyStack") > 0 ||
      defMap_.count(structPath + "/DestroyHeap") > 0 || defMap_.count(structPath + "/DestroyBuffer") > 0 ||
      defMap_.count(structPath + "/Copy") > 0 || defMap_.count(structPath + "/Move") > 0) {
    return false;
  }

  const Definition *structDef = nullptr;
  auto defIt = defMap_.find(structPath);
  if (defIt != defMap_.end()) {
    structDef = defIt->second;
  }
  if (structDef == nullptr) {
    return true;
  }

  for (const auto &fieldStmt : structDef->statements) {
    if (!fieldStmt.isBinding) {
      continue;
    }
    BindingInfo fieldBinding;
    if (!resolveStructFieldBinding(*structDef, fieldStmt, fieldBinding)) {
      continue;
    }
    if (!isRelocationTrivialContainerElementType(bindingTypeText(fieldBinding),
                                                 structDef->namespacePrefix,
                                                 &structDef->templateArgs,
                                                 visitingStructs)) {
      return false;
    }
  }

  return true;
}

bool SemanticsValidator::validateVectorIndexedRemovalHelperElementType(
    const BindingInfo &binding,
    const std::string &helperName,
    const std::string &namespacePrefix,
    const std::vector<std::string> *definitionTemplateArgs) {
  auto failContainerHelperDiagnostic = [&](std::string message) -> bool {
    if (currentDefinitionContext_ != nullptr) {
      return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));
    }
    return failUncontextualizedDiagnostic(std::move(message));
  };
  std::string experimentalElemType;
  const bool requiresDropTrivial = helperName != "remove_swap" && helperName != "remove_at";
  if (requiresDropTrivial && !extractExperimentalVectorElementType(binding, experimentalElemType) &&
      currentValidationState_.context.definitionPath.rfind("/std/collections/experimental_vector/", 0) != 0 &&
      namespacePrefix.rfind("/std/collections/experimental_vector", 0) != 0 &&
      !binding.typeTemplateArg.empty()) {
    std::unordered_set<std::string> visitingStructs;
    if (!isDropTrivialContainerElementType(binding.typeTemplateArg,
                                           namespacePrefix,
                                           definitionTemplateArgs,
                                           visitingStructs)) {
      return failContainerHelperDiagnostic(
          helperName +
          " requires drop-trivial vector element type until container drop semantics are implemented: " +
          binding.typeTemplateArg);
    }
  }
  return validateVectorRelocationHelperElementType(binding, helperName, namespacePrefix, definitionTemplateArgs);
}

bool SemanticsValidator::validateVectorRelocationHelperElementType(
    const BindingInfo &binding,
    const std::string &helperName,
    const std::string &namespacePrefix,
    const std::vector<std::string> *definitionTemplateArgs) {
  auto failContainerHelperDiagnostic = [&](std::string message) -> bool {
    if (currentDefinitionContext_ != nullptr) {
      return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));
    }
    return failUncontextualizedDiagnostic(std::move(message));
  };
  if (helperName == "remove_swap" || helperName == "remove_at") {
    return true;
  }
  std::string experimentalElemType;
  if (extractExperimentalVectorElementType(binding, experimentalElemType)) {
    return true;
  }
  if (currentValidationState_.context.definitionPath.rfind("/std/collections/experimental_vector/", 0) == 0 ||
      namespacePrefix.rfind("/std/collections/experimental_vector", 0) == 0) {
    return true;
  }
  if (binding.typeTemplateArg.empty()) {
    return true;
  }

  std::unordered_set<std::string> visitingStructs;
  if (isRelocationTrivialContainerElementType(binding.typeTemplateArg,
                                              namespacePrefix,
                                              definitionTemplateArgs,
                                              visitingStructs)) {
    return true;
  }

  return failContainerHelperDiagnostic(
      helperName +
      " requires relocation-trivial vector element type until container move/reallocation semantics are "
      "implemented: " +
      binding.typeTemplateArg);
}

} // namespace primec::semantics
