#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::inferResolvedDirectCallBindingType(const std::string &resolvedPath,
                                                            BindingInfo &bindingOut) const {
  auto inferDeclaredCollectionBinding = [&](const Definition &definition) -> bool {
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizedReturnType, base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return false;
      }
      const std::string normalizedCollectionType = normalizeCollectionTypePath(base);
      if (((base == "array" || base == "vector" || base == "soa_vector") ||
           normalizedCollectionType == "/vector") &&
          args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if (isMapCollectionTypeName(base) && args.size() == 2) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = args.front();
        return true;
      }
      return false;
    }
    return false;
  };

  if (resolvedPath.empty()) {
    return false;
  }

  const auto defIt = defMap_.find(resolvedPath);
  if (defIt != defMap_.end() && defIt->second != nullptr &&
      inferDeclaredCollectionBinding(*defIt->second)) {
    return true;
  }

  const auto directBindingIt = returnBindings_.find(resolvedPath);
  if (directBindingIt != returnBindings_.end() && !directBindingIt->second.typeName.empty()) {
    bindingOut = directBindingIt->second;
    return true;
  }

  const auto directStructIt = returnStructs_.find(resolvedPath);
  if (directStructIt != returnStructs_.end() && !directStructIt->second.empty()) {
    if (directStructIt->second.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
      bindingOut.typeName = directStructIt->second;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (!normalizeCollectionTypePath(directStructIt->second).empty()) {
      if (defIt != defMap_.end() && defIt->second != nullptr &&
          inferDeclaredCollectionBinding(*defIt->second)) {
        return true;
      }
      return false;
    }
    bindingOut.typeName = directStructIt->second;
    bindingOut.typeTemplateArg.clear();
    return true;
  }

  if (defIt != defMap_.end() && defIt->second != nullptr) {
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnTypeText = normalizeBindingTypeName(transform.templateArgs.front());
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedReturnTypeText, base, argText)) {
        continue;
      }
      if (normalizeCollectionTypePath(normalizedReturnTypeText).empty() &&
          normalizeBindingTypeName(normalizedReturnTypeText) != "Pointer" &&
          normalizeBindingTypeName(normalizedReturnTypeText) != "Reference" &&
          returnKindForTypeName(normalizedReturnTypeText) == ReturnKind::Unknown) {
        bindingOut.typeName = normalizedReturnTypeText;
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      break;
    }
  }

  const auto kindIt = returnKinds_.find(resolvedPath);
  if (kindIt == returnKinds_.end()) {
    return false;
  }
  if (kindIt->second == ReturnKind::Unknown || kindIt->second == ReturnKind::Void) {
    return false;
  }
  if (kindIt->second == ReturnKind::Array) {
    if (defIt != defMap_.end() && defIt->second != nullptr &&
        inferDeclaredCollectionBinding(*defIt->second)) {
      return true;
    }
    const auto structIt = returnStructs_.find(resolvedPath);
    if (structIt == returnStructs_.end() || structIt->second.empty()) {
      return false;
    }
    bindingOut.typeName = structIt->second;
    bindingOut.typeTemplateArg.clear();
    return true;
  }

  const std::string inferredType = typeNameForReturnKind(kindIt->second);
  if (inferredType.empty()) {
    return false;
  }
  bindingOut.typeName = inferredType;
  bindingOut.typeTemplateArg.clear();
  return true;
}

}  // namespace primec::semantics
