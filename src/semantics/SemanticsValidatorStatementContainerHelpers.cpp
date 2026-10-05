#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

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

bool isLegacyExperimentalVectorValidationContext(std::string_view definitionPath,
                                                 std::string_view namespacePrefix) {
  return definitionPath.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 ||
         namespacePrefix.rfind(legacyExperimentalVectorCompatibilityRoot(), 0) == 0;
}

bool isExperimentalKeyValueBackingStructPath(std::string_view structPath) {
  const std::string_view normalizedPath = trimLeadingSlash(structPath);
  return isExperimentalCollectionBackingTypeName("map", "Map", normalizedPath) ||
         isExperimentalCollectionBackingTypeName("map", "Entry", normalizedPath);
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
    if (normalizedBase == "vector" || normalizedBase == "map" || normalizedBase == "soa" ||
        normalizedBase == "uninitialized" || normalizedBase == "Buffer") {
      return false;
    }
    base = normalizedBase;
  } else {
    base = normalizedType;
  }

  const std::string structPath = resolveStructTypePath(base, namespacePrefix, structNames_);
  if (isExperimentalKeyValueBackingStructPath(structPath)) {
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
    if (!fieldStmt.isBinding || isCompileTimeTypeBinding(fieldStmt)) {
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
    if (normalizedBase == "vector" || normalizedBase == "map" || normalizedBase == "soa" ||
        normalizedBase == "uninitialized" || normalizedBase == "Buffer") {
      return false;
    }
    base = normalizedBase;
  } else {
    base = normalizedType;
  }

  const std::string structPath = resolveStructTypePath(base, namespacePrefix, structNames_);
  if (isExperimentalKeyValueBackingStructPath(structPath)) {
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
    if (!fieldStmt.isBinding || isCompileTimeTypeBinding(fieldStmt)) {
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
  if (requiresDropTrivial && !extractCollectionVectorElementType(binding, experimentalElemType) &&
      !isLegacyExperimentalVectorValidationContext(
          currentValidationState_.context.definitionPath,
          namespacePrefix) &&
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
  if (extractCollectionVectorElementType(binding, experimentalElemType)) {
    return true;
  }
  if (isLegacyExperimentalVectorValidationContext(
          currentValidationState_.context.definitionPath,
          namespacePrefix)) {
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

bool SemanticsValidator::isOwningBorrowedParameter(const std::vector<ParameterInfo> &params,
                                                   const Expr &expr,
                                                   const std::string &namespacePrefix) {
  if (expr.kind != Expr::Kind::Name || expr.name == "this" ||
      currentValidationState_.context.definitionIsUnsafe) {
    return false;
  }
  const BindingInfo *paramBinding = findParamBinding(params, expr.name);
  if (paramBinding == nullptr || paramBinding->isCopy || paramBinding->isMove) {
    return false;
  }
  return bindingOwnsResources(*paramBinding, namespacePrefix);
}

void SemanticsValidator::markContainerViewBinding(
    const std::vector<ParameterInfo> &params,
    std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &bindingStmt) {
  if (!bindingStmt.isBinding || bindingStmt.args.size() != 1) {
    return;
  }
  auto bindingIt = locals.find(bindingStmt.name);
  if (bindingIt == locals.end()) {
    return;
  }
  BindingInfo &binding = bindingIt->second;
  if (binding.isMutable || !binding.referenceRoot.empty()) {
    return;
  }
  // Collection types (declared `collection_type` / `key_value_type`) bound from a place share the
  // place's storage.
  std::string namespacePrefix;
  if (const auto defIt = defMap_.find(currentValidationState_.context.definitionPath);
      defIt != defMap_.end()) {
    namespacePrefix = defIt->second->namespacePrefix;
  }
  const std::string typeName = normalizeBindingTypeName(binding.typeName);
  const auto family = collection_helpers::parseCollectionFamily("/" + typeName);
  const bool isBuiltinCollection = family == collection_helpers::CollectionFamily::Vector ||
                                   family == collection_helpers::CollectionFamily::Map ||
                                   family == collection_helpers::CollectionFamily::Soa;
  const std::string structPath = resolveStructTypePath(typeName, namespacePrefix, structNames_);
  const auto structIt = defMap_.find(structPath);
  const bool isCollectionStruct = structIt != defMap_.end() && structIt->second != nullptr &&
                                  std::any_of(structIt->second->transforms.begin(),
                                              structIt->second->transforms.end(),
                                              [](const Transform &transform) {
                                                return transform.name == "collection_type" ||
                                                       transform.name == "key_value_type";
                                              });
  if (!isBuiltinCollection && !isCollectionStruct) {
    return;
  }
  const Expr *source = &bindingStmt.args.front();
  while (source->kind == Expr::Kind::Call && source->isFieldAccess && source->args.size() == 1) {
    source = &source->args.front();
  }
  if (source->kind != Expr::Kind::Name) {
    return;
  }
  const BindingInfo *sourceBinding = findParamBinding(params, source->name);
  if (sourceBinding == nullptr) {
    const auto sourceIt = locals.find(source->name);
    if (sourceIt == locals.end()) {
      return;
    }
    sourceBinding = &sourceIt->second;
  }
  binding.isContainerView = true;
  binding.referenceRoot = sourceBinding->isContainerView && !sourceBinding->referenceRoot.empty()
                              ? sourceBinding->referenceRoot
                              : source->name;
}

std::string
SemanticsValidator::liveContainerViewOf(const std::vector<ParameterInfo> &params,
                                        const std::unordered_map<std::string, BindingInfo> &locals,
                                        const std::string &rootName) {
  (void)params;
  if (rootName.empty() || currentValidationState_.context.definitionIsUnsafe) {
    return {};
  }
  std::string viewName;
  for (const auto &[name, binding] : locals) {
    if (binding.isContainerView && binding.referenceRoot == rootName &&
        currentValidationState_.endedReferenceBorrows.count(name) == 0 &&
        (viewName.empty() || name < viewName)) {
      viewName = name;
    }
  }
  return viewName;
}

bool SemanticsValidator::bindingOwnsResources(const BindingInfo &binding,
                                              const std::string &namespacePrefix) {
  const std::vector<std::string> *definitionTemplateArgs = nullptr;
  std::string definitionNamespacePrefix = namespacePrefix;
  if (const auto defIt = defMap_.find(currentValidationState_.context.definitionPath);
      defIt != defMap_.end()) {
    definitionTemplateArgs = &defIt->second->templateArgs;
    if (definitionNamespacePrefix.empty()) {
      definitionNamespacePrefix = defIt->second->namespacePrefix;
    }
  }
  // A type owns resources when it is a container, defines `Destroy`, or holds such a type; values
  // of other types (scalars, strings, pointers, references, plain structs) copy safely.
  std::unordered_set<std::string> visitingStructs;
  std::function<bool(const std::string &, const std::string &, const std::vector<std::string> *)>
      ownsResources = [&](const std::string &typeName,
                          const std::string &typeNamespace,
                          const std::vector<std::string> *templateArgs) -> bool {
    if (templateArgsContainTypeName(templateArgs, typeName)) {
      return false;
    }
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    std::string base = normalizedType;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      base = normalizeBindingTypeName(base);
      if (base == "Pointer" || base == "Reference") {
        return false;
      }
      if (base == "vector" || base == "map" || base == "soa" || base == "uninitialized" ||
          base == "Buffer") {
        return true;
      }
      if (base == "array") {
        std::vector<std::string> args;
        return splitTopLevelTemplateArgs(argText, args) && args.size() == 1 &&
               ownsResources(args.front(), typeNamespace, templateArgs);
      }
    }
    const std::string structPath = resolveStructTypePath(base, typeNamespace, structNames_);
    if (structPath.empty() || structNames_.count(structPath) == 0 ||
        !visitingStructs.insert(structPath).second) {
      return false;
    }
    if (defMap_.count(structPath + "/Destroy") > 0 ||
        defMap_.count(structPath + "/DestroyStack") > 0 ||
        defMap_.count(structPath + "/DestroyHeap") > 0 ||
        defMap_.count(structPath + "/DestroyBuffer") > 0) {
      return true;
    }
    const auto defIt = defMap_.find(structPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    const Definition &structDef = *defIt->second;
    for (const auto &fieldStmt : structDef.statements) {
      if (!fieldStmt.isBinding || isCompileTimeTypeBinding(fieldStmt)) {
        continue;
      }
      BindingInfo fieldBinding;
      if (resolveStructFieldBinding(structDef, fieldStmt, fieldBinding) &&
          ownsResources(
              bindingTypeText(fieldBinding), structDef.namespacePrefix, &structDef.templateArgs)) {
        return true;
      }
    }
    return false;
  };
  const std::string typeText = expectedBindingTypeText(binding);
  return !typeText.empty() &&
         ownsResources(typeText, definitionNamespacePrefix, definitionTemplateArgs);
}

} // namespace primec::semantics
