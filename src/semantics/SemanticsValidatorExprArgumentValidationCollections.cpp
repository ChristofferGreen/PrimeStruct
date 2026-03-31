#include "SemanticsValidator.h"

#include <optional>
#include <string>

namespace primec::semantics {

bool SemanticsValidator::isBuiltinCollectionLiteralExpr(const Expr &candidate) const {
  if (candidate.kind != Expr::Kind::Call) {
    return false;
  }
  if (defMap_.find(resolveCalleePath(candidate)) != defMap_.end()) {
    return false;
  }
  std::string collectionName;
  return getBuiltinCollectionName(candidate, collectionName);
}

bool SemanticsValidator::isStringExprForArgumentValidation(
    const Expr &arg,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (dispatchResolvers.resolveStringTarget != nullptr &&
      dispatchResolvers.resolveStringTarget(arg)) {
    return true;
  }
  if (arg.kind == Expr::Kind::Call) {
    const std::string resolvedPath = resolveCalleePath(arg);
    const bool treatAsBuiltinAccess =
        defMap_.find(resolvedPath) == defMap_.end() ||
        resolvedPath.rfind("/std/collections/vector/at", 0) == 0 ||
        resolvedPath == "/map/at" ||
        resolvedPath == "/map/at_unsafe" ||
        resolvedPath == "/std/collections/map/at" ||
        resolvedPath == "/std/collections/map/at_unsafe";
    std::string accessName;
    if (treatAsBuiltinAccess &&
        getBuiltinArrayAccessName(arg, accessName) &&
        arg.args.size() == 2) {
      std::string mapValueType;
      if (resolveMapValueType(arg.args.front(), dispatchResolvers, mapValueType) &&
          normalizeBindingTypeName(mapValueType) == "string") {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::extractExperimentalMapFieldTypesFromStructPath(
    const std::string &structPath,
    std::string &keyTypeOut,
    std::string &valueTypeOut) const {
  keyTypeOut.clear();
  valueTypeOut.clear();
  if (structPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding) {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    std::string elemType;
    if (normalizeBindingTypeName(fieldBinding.typeName) == "vector" &&
        !fieldBinding.typeTemplateArg.empty()) {
      elemType = fieldBinding.typeTemplateArg;
    } else if (!extractExperimentalVectorElementType(fieldBinding, elemType)) {
      continue;
    }
    if (fieldExpr.name == "keys") {
      keyTypeOut = elemType;
    } else if (fieldExpr.name == "payloads") {
      valueTypeOut = elemType;
    }
  }
  return !keyTypeOut.empty() && !valueTypeOut.empty();
}

bool SemanticsValidator::extractExperimentalVectorElementTypeFromStructPath(
    const std::string &structPath,
    std::string &elemTypeOut) const {
  elemTypeOut.clear();
  if (structPath.rfind("/std/collections/experimental_vector/Vector__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    if (normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
        fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                               pointeeBase,
                               pointeeArgText) ||
        normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    std::vector<std::string> pointeeArgs;
    if (!splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) ||
        pointeeArgs.size() != 1) {
      continue;
    }
    elemTypeOut = pointeeArgs.front();
    return true;
  }
  return false;
}

bool SemanticsValidator::extractExperimentalVectorElementType(const BindingInfo &binding,
                                                              std::string &elemTypeOut) const {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        std::string normalizedBase = base;
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if ((normalizedBase == "Vector" ||
             normalizedBase == "std/collections/experimental_vector/Vector") &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (normalizedResolvedPath.rfind("std/collections/experimental_vector/Vector__", 0) != 0) {
        return false;
      }
      return extractExperimentalVectorElementTypeFromStructPath(resolvedPath, elemTypeOut);
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

bool SemanticsValidator::extractExperimentalSoaVectorElementTypeFromStructPath(
    const std::string &structPath,
    std::string &elemTypeOut) const {
  elemTypeOut.clear();
  if (structPath.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "storage") {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    std::string normalizedFieldType = normalizeBindingTypeName(fieldBinding.typeName);
    if (!normalizedFieldType.empty() && normalizedFieldType.front() == '/') {
      normalizedFieldType.erase(normalizedFieldType.begin());
    }
    if (!fieldBinding.typeTemplateArg.empty() &&
        (normalizedFieldType == "SoaColumn" ||
         normalizedFieldType == "std/collections/experimental_soa_storage/SoaColumn")) {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, args) || args.size() != 1) {
        continue;
      }
      elemTypeOut = args.front();
      return true;
    }
    std::string resolvedFieldPath = normalizeBindingTypeName(fieldBinding.typeName);
    if (!resolvedFieldPath.empty() && resolvedFieldPath.front() != '/') {
      resolvedFieldPath.insert(resolvedFieldPath.begin(), '/');
    }
    if (normalizedFieldType.rfind("std/collections/experimental_soa_storage/SoaColumn__", 0) != 0 ||
        !extractExperimentalSoaColumnElementTypeFromStructPath(resolvedFieldPath, elemTypeOut)) {
      continue;
    }
    return true;
  }
  return false;
}

bool SemanticsValidator::extractExperimentalSoaColumnElementTypeFromStructPath(
    const std::string &structPath,
    std::string &elemTypeOut) const {
  elemTypeOut.clear();
  if (structPath.rfind("/std/collections/experimental_soa_storage/SoaColumn__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    if (normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
        fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                               pointeeBase,
                               pointeeArgText) ||
        normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    std::vector<std::string> pointeeArgs;
    if (!splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) ||
        pointeeArgs.size() != 1) {
      continue;
    }
    elemTypeOut = pointeeArgs.front();
    return true;
  }
  return false;
}

bool SemanticsValidator::extractExperimentalSoaVectorElementType(const BindingInfo &binding,
                                                                 std::string &elemTypeOut) const {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        std::string normalizedBase = base;
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if ((normalizedBase == "SoaVector" ||
             normalizedBase == "std/collections/experimental_soa_vector/SoaVector") &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (normalizedResolvedPath.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) != 0) {
        return false;
      }
      return extractExperimentalSoaVectorElementTypeFromStructPath(resolvedPath, elemTypeOut);
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

} // namespace primec::semantics
