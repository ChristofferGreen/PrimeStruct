#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::inferResolvedDirectCallBindingType(const std::string &resolvedPath,
                                                            BindingInfo &bindingOut) const {
  auto inferDeclaredCollectionBinding = [&](const Definition &definition) -> bool {
    auto isSupportedCollectionType = [&](const std::string &typeName) -> bool {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(typeName), base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return false;
      }
      return ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) ||
             (isMapCollectionTypeName(base) && args.size() == 2);
    };

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
      if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if (isMapCollectionTypeName(base) && args.size() == 2) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if ((base == "Reference" || base == "Pointer") && args.size() == 1 &&
          isSupportedCollectionType(args.front())) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = args.front();
        return true;
      }
      return false;
    }
    return false;
  };

  const auto defIt = defMap_.find(resolvedPath);
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
